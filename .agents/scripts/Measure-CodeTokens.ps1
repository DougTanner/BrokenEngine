[CmdletBinding()]
param(
	[Parameter(Mandatory = $true, Position = 0)]
	[ValidateNotNullOrEmpty()]
	[string[]] $Path
)

$ErrorActionPreference = 'Stop'

function Test-RawDelimiterCharacter([char] $Character) {
	$value = [int]$Character
	return ($value -ge 0x21 -and $value -le 0x23) -or
		($value -ge 0x25 -and $value -le 0x27) -or
		($value -ge 0x2A -and $value -le 0x3F) -or
		($value -ge 0x41 -and $value -le 0x5B) -or
		($value -ge 0x5D -and $value -le 0x5F) -or
		($value -ge 0x61 -and $value -le 0x7E)
}

function Get-CommentFreeText([string] $Text) {
	$builder = [Text.StringBuilder]::new($Text.Length)
	$index = 0
	while ($index -lt $Text.Length)
	{
		if ($index + 1 -lt $Text.Length -and $Text[$index] -eq '/' -and $Text[$index + 1] -eq '/')
		{
			$index += 2
			while ($index -lt $Text.Length -and $Text[$index] -ne "`n") { $index++ }
			if ($index -lt $Text.Length)
			{
				[void]$builder.Append("`n")
				$index++
			}
			continue
		}

		if ($index + 1 -lt $Text.Length -and $Text[$index] -eq '/' -and $Text[$index + 1] -eq '*')
		{
			[void]$builder.Append(' ')
			$index += 2
			while ($index + 1 -lt $Text.Length -and -not ($Text[$index] -eq '*' -and $Text[$index + 1] -eq '/')) { $index++ }
			if ($index + 1 -lt $Text.Length) { $index += 2 } else { $index = $Text.Length }
			continue
		}

		$isRawIntroducer = $false
		if ($index + 1 -lt $Text.Length -and $Text[$index] -eq 'R' -and $Text[$index + 1] -eq '"')
		{
			$isRawIntroducer = $index -eq 0 -or -not ([char]::IsLetterOrDigit($Text[$index - 1]) -or $Text[$index - 1] -eq '_')
			if (-not $isRawIntroducer -and $Text[$index - 1] -in @('u', 'U', 'L'))
			{
				$isRawIntroducer = $index -eq 1 -or -not ([char]::IsLetterOrDigit($Text[$index - 2]) -or $Text[$index - 2] -eq '_')
			}
			if (-not $isRawIntroducer -and $index -ge 2 -and $Text[$index - 2] -eq 'u' -and $Text[$index - 1] -eq '8')
			{
				$isRawIntroducer = $index -eq 2 -or -not ([char]::IsLetterOrDigit($Text[$index - 3]) -or $Text[$index - 3] -eq '_')
			}
		}

		if ($isRawIntroducer)
		{
			$delimiterStart = $index + 2
			$delimiterEnd = $delimiterStart
			while ($delimiterEnd -lt $Text.Length -and $delimiterEnd - $delimiterStart -le 16 -and
				$Text[$delimiterEnd] -ne '(' -and (Test-RawDelimiterCharacter $Text[$delimiterEnd]))
			{
				$delimiterEnd++
			}

			if ($delimiterEnd -lt $Text.Length -and $Text[$delimiterEnd] -eq '(' -and $delimiterEnd - $delimiterStart -le 16)
			{
				$delimiter = $Text.Substring($delimiterStart, $delimiterEnd - $delimiterStart)
				$terminator = ')' + $delimiter + '"'
				$literalEnd = $Text.IndexOf($terminator, $delimiterEnd + 1, [StringComparison]::Ordinal)
				if ($literalEnd -lt 0) { $literalEnd = $Text.Length } else { $literalEnd += $terminator.Length }
				[void]$builder.Append($Text, $index, $literalEnd - $index)
				$index = $literalEnd
				continue
			}
		}

		if ($Text[$index] -eq '"' -or $Text[$index] -eq "'")
		{
			$quote = $Text[$index]
			[void]$builder.Append($quote)
			$index++
			while ($index -lt $Text.Length)
			{
				$character = $Text[$index]
				[void]$builder.Append($character)
				$index++
				if ($character -eq [char]'\' -and $index -lt $Text.Length)
				{
					[void]$builder.Append($Text[$index])
					$index++
					continue
				}
				if ($character -eq $quote) { break }
			}
			continue
		}

		[void]$builder.Append($Text[$index])
		$index++
	}

	return $builder.ToString()
}

try
{
	Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force
	$script:CodeExtensions = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	foreach ($extension in @('.cpp', '.h', '.hpp', '.inl', '.c', '.comp', '.vert', '.frag', '.geom', '.tesc', '.tese', '.glsl'))
	{
		[void]$script:CodeExtensions.Add($extension)
	}

	$folders = [Collections.Generic.List[object]]::new()
	foreach ($inputPath in $Path)
	{
		$item = Get-Item -LiteralPath $inputPath
		if (-not $item.PSIsContainer)
		{
			throw "Path is not a directory: $inputPath"
		}

		[string[]] $files = @(Get-ChildItem -LiteralPath $item.FullName -File |
			Where-Object { $script:CodeExtensions.Contains($_.Extension) } |
			ForEach-Object { $_.FullName })
		[Array]::Sort($files, [StringComparer]::Ordinal)

		[int64] $tokens = 0
		foreach ($file in $files)
		{
			$text = Get-AgentNormalizedText ([IO.File]::ReadAllBytes($file))
			$tokens += Measure-AgentTokenCount (Get-CommentFreeText $text)
		}

		$folders.Add([pscustomobject][ordered]@{
			path = Get-AgentCanonicalPath $item.FullName
			codeFileCount = $files.Count
			tokens = $tokens
		})
	}

	[pscustomobject][ordered]@{
		metric = 'bt-token-v1'
		folders = @($folders)
	} | ConvertTo-Json -Depth 4
}
catch
{
	Write-Error $_
	exit 1
}
