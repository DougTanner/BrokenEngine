Set-StrictMode -Version Latest

# The single implementation of the whole-file BT_CLIENT/BT_SERVER affinity rule, shared by
# /update-vcxproj's membership resolver and by the session change inventory's vcxproj candidates
# so the two can never disagree about what counts as a one-sided file.

# Strips comments so a line's remaining code text decides whether it is substantive.
# String and character literals are skipped whole so a literal '/*' cannot open a comment.
function Get-CodeText
{
	param([string] $Line, [ref] $InBlockComment)

	$builder = [Text.StringBuilder]::new()
	$index = 0
	while ($index -lt $Line.Length)
	{
		$character = $Line[$index]
		if ($InBlockComment.Value)
		{
			if ($character -eq '*' -and ($index + 1) -lt $Line.Length -and $Line[$index + 1] -eq '/')
			{
				$InBlockComment.Value = $false
				$index += 2
				continue
			}

			$index++
			continue
		}

		if ($character -eq '/' -and ($index + 1) -lt $Line.Length)
		{
			if ($Line[$index + 1] -eq '/')
			{
				break
			}
			if ($Line[$index + 1] -eq '*')
			{
				$InBlockComment.Value = $true
				$index += 2
				continue
			}
		}

		if ($character -eq '"' -or $character -eq "'")
		{
			$quote = $character
			$null = $builder.Append($character)
			$index++
			while ($index -lt $Line.Length)
			{
				$inner = $Line[$index]
				$null = $builder.Append($inner)
				$index++
				if ($inner -eq '\' -and $index -lt $Line.Length)
				{
					$null = $builder.Append($Line[$index])
					$index++
					continue
				}
				if ($inner -eq $quote)
				{
					break
				}
			}

			continue
		}

		$null = $builder.Append($character)
		$index++
	}

	return $builder.ToString()
}

# Implements the SKILL.md whole-file affinity rule literally: skip the BOM, whitespace,
# comment, '#pragma once' and include prologue; require the exact guard form as the first
# substantive directive; track nesting to find that guard's matching '#endif'; require no
# substantive content after it.
function Resolve-FileAffinity
{
	param([string] $Text)

	$lines = $Text -split "\r?\n"
	$codeLines = [string[]]::new($lines.Length)
	$inBlockComment = $false
	for ($index = 0; $index -lt $lines.Length; $index++)
	{
		$codeLines[$index] = (Get-CodeText $lines[$index] ([ref]$inBlockComment)).Trim()
	}

	if ($inBlockComment)
	{
		return [ordered]@{
			affinity = 'unprovable'
			code = 'affinity.malformed-conditional'
			detail = 'An unterminated block comment leaves the file text ambiguous.'
		}
	}

	# One entry per open conditional block, holding whether that block already saw its #else,
	# so a second #else or an #elif after #else is caught at any nesting depth.
	$elseSeen = [System.Collections.Generic.List[bool]]::new()
	foreach ($codeLine in $codeLines)
	{
		if ($codeLine -cmatch '^#\s*(if|ifdef|ifndef)\b')
		{
			$elseSeen.Add($false)
		}
		elseif ($codeLine -cmatch '^#\s*(else|elif)\b')
		{
			if ($elseSeen.Count -eq 0)
			{
				return [ordered]@{
					affinity = 'unprovable'
					code = 'affinity.unbalanced-conditional'
					detail = 'An #else or #elif appears outside any conditional block.'
				}
			}

			if ($elseSeen[$elseSeen.Count - 1])
			{
				return [ordered]@{
					affinity = 'unprovable'
					code = 'affinity.malformed-conditional'
					detail = 'A conditional block carries a second #else or an #elif after its #else.'
				}
			}

			if ($codeLine -cmatch '^#\s*else\b')
			{
				$elseSeen[$elseSeen.Count - 1] = $true
			}
		}
		elseif ($codeLine -cmatch '^#\s*endif\b')
		{
			if ($elseSeen.Count -eq 0)
			{
				return [ordered]@{
					affinity = 'unprovable'
					code = 'affinity.unbalanced-conditional'
					detail = 'An #endif appears with no matching #if.'
				}
			}

			$elseSeen.RemoveAt($elseSeen.Count - 1)
		}
	}

	if ($elseSeen.Count -ne 0)
	{
		return [ordered]@{
			affinity = 'unprovable'
			code = 'affinity.unbalanced-conditional'
			detail = "$($elseSeen.Count) conditional block(s) are never closed by a matching #endif."
		}
	}

	$firstSubstantive = -1
	for ($index = 0; $index -lt $codeLines.Length; $index++)
	{
		$codeLine = $codeLines[$index]
		if ($codeLine.Length -eq 0 -or $codeLine -cmatch '^#\s*pragma\s+once\b' -or $codeLine -cmatch '^#\s*include\b')
		{
			continue
		}

		$firstSubstantive = $index
		break
	}

	if ($firstSubstantive -lt 0)
	{
		return [ordered]@{
			affinity = 'shared'
			code = 'affinity.no-whole-file-guard'
			detail = 'The file holds no substantive directive after its prologue.'
		}
	}

	$guardLine = $codeLines[$firstSubstantive]
	$guarded = ''
	if ($guardLine -ceq '#if defined(BT_CLIENT)')
	{
		$guarded = 'client'
	}
	elseif ($guardLine -ceq '#if defined(BT_SERVER)')
	{
		$guarded = 'server'
	}
	else
	{
		return [ordered]@{
			affinity = 'shared'
			code = 'affinity.no-whole-file-guard'
			detail = 'The first substantive directive is not an exact BT_CLIENT/BT_SERVER guard.'
		}
	}

	$depth = 1
	$guardEnd = -1
	for ($index = $firstSubstantive + 1; $index -lt $codeLines.Length; $index++)
	{
		$codeLine = $codeLines[$index]
		if ($codeLine -cmatch '^#\s*(if|ifdef|ifndef)\b')
		{
			$depth++
		}
		elseif ($depth -eq 1 -and $codeLine -cmatch '^#\s*(else|elif)\b')
		{
			return [ordered]@{
				affinity = 'shared'
				code = 'affinity.guard-has-alternative-branch'
				detail = 'The leading guard carries an #else/#elif branch, so it excludes no whole file.'
			}
		}
		elseif ($codeLine -cmatch '^#\s*endif\b')
		{
			$depth--
			if ($depth -eq 0)
			{
				$guardEnd = $index
				break
			}
		}
	}

	for ($index = $guardEnd + 1; $index -lt $codeLines.Length; $index++)
	{
		if ($codeLines[$index].Length -gt 0)
		{
			return [ordered]@{
				affinity = 'shared'
				code = 'affinity.content-after-guard'
				detail = 'Substantive content follows the guard, so the guard does not enclose the whole file.'
			}
		}
	}

	return [ordered]@{
		affinity = $guarded
		code = "affinity.whole-file-$guarded-guard"
		detail = 'The exact guard is the first substantive directive and its matching #endif is last.'
	}
}

Export-ModuleMember -Function Resolve-FileAffinity
