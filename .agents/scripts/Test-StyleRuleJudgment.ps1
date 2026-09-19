# Style-rule judgment for /code-style-review's hand-read rules: takes C++ blocks (one function or class body
# each), asks Jev through Invoke-Jev.ps1 one yes/no question per rule over each block plus one question per
# identifier for the abbreviation rule, and reports per block the rules whose probability reaches its
# threshold, ordered by probability. The questions name the forbidden construct and this codebase's own
# exceptions; pasting the guide's rule text makes the model answer "does this rule apply" instead of "is it
# broken" (Documents/Investigations/JevStyleRuleJudgment.md). Rule 61 is deliberately absent: the Allman
# brace on its own line reads as "no brace" to the model, and a two-line scanner decides it exactly.
#
# -CasesPath measures the questions against a labelled corpus: each case carries `id`, `code` (an array of
# lines), and `expected` (the `rule<n>` keys that should fire), and the result adds per-rule hit counts.
# Without a key or a reachable service the result is `blocked` and no block was judged, which is the
# behaviour the review has without Jev.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $CasesPath,
	[string] $OutputPath,
	[double] $BlockThreshold = 0.5,
	[double] $Rule3Threshold = 0.9,
	[double] $NameThreshold = 0.7
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Utf8 = [Text.UTF8Encoding]::new($false)

# One question per block-level rule. Each instruction names the construct the rule forbids and the correct
# forms the model must not flag; the criteria restate both so the boundary is explicit.
$script:BlockQuestions = [ordered]@{
	rule3 = [ordered]@{
		instructions = 'Is there a variable in `code` whose name lacks the Hungarian prefix its type requires, or carries a prefix for a different type? Required prefixes: k constexpr, s static, g global, m class member (classes only, never struct members), p pointer, r reference, c char, i signed integer, ui unsigned integer, b bool, e enum value, f float or double, f2/f3/f4 XMFLOAT, vec XMVECTOR, mat XMMATRIX. These take NO prefix and are never violations: std::string and std::string_view, containers (std::vector, std::array, maps, sets), std::span, iterators, common::crc_t values (named crc), Vulkan handles (named with a vk prefix or Vk suffix), and instances of classes or structs. Function names are not judged.'
		true = 'At least one variable name is missing its required prefix (like `int64_t count`, `bool done`, `Unit* unit`) or has a prefix that contradicts its type (like `bool iEnabled`)'
		false = 'Every variable name carries the prefix its type requires, or there are no variable declarations to judge'
	}
	rule14 = [ordered]@{
		instructions = 'Does an identifier in `code` use "Num" as a word meaning a count, where "Count" is required (kuiNumThreads is wrong, kuiThreadCount is right)? "Num" inside an ordinary English word such as Numerator, Number, or Enumerated is not a violation.'
		true = 'An identifier uses Num as a standalone word for a count, such as iNumUnits or kuiNumThreads'
		false = 'No identifier uses Num for a count; Number, Numerator, and similar whole words do not count'
	}
	rule16 = [ordered]@{
		instructions = 'Does `code` index a std::map, std::unordered_map, or std::vector with operator[] (square brackets) in actual code, rather than .at(), insert_or_assign, or try_emplace? Square brackets inside a comment or a string literal, on a C array, on a std::span, or on a std::array are not violations.'
		true = 'A map or vector is indexed with square brackets in executable code, such as mTextureMap[crc] or mIslands[i]'
		false = 'Every map and vector access uses .at(), insert_or_assign, try_emplace, find, or iteration; any square brackets are on arrays, spans, comments, or strings'
	}
	rule21 = [ordered]@{
		instructions = 'Does a function declaration or definition in `code` take a raw pointer parameter paired with a separate size or count parameter (such as `const uint8_t* pData, size_t uiSize`) where std::span should be used, or does the code use std::bitset?'
		true = 'A pointer-plus-size parameter pair or a std::bitset is present'
		false = 'No pointer-plus-size parameter pair and no std::bitset; a lone pointer parameter without a size, or a std::span, is fine'
	}
	rule41 = [ordered]@{
		instructions = 'Does `code` name a C++ standard-library type or function without its std:: prefix, such as bare `vector`, `string`, `unordered_map`, `min`, or `move`? Types from other namespaces, engine types, Vulkan types, and DirectX Math types are not standard-library names. The fixed-width and size types size_t, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, and uint64_t are written without std:: by convention and are never violations. A line that already writes std:: is not a violation.'
		true = 'A standard-library type or function appears without the std:: prefix'
		false = 'Every standard-library name that appears is written with std::, or no standard-library name appears'
	}
	rule49 = [ordered]@{
		instructions = 'Does `code` define a trivial accessor or pass-through function: a getter, setter, Is*, Can*, drain, or take method that reads or writes one piece of state, or any function whose entire body is a single return of state, a single assignment, or a single call forwarded to another object? Spreading that one statement over several lines does not change the answer. A function whose body performs several statements that must happen together, or a serialization or codec adapter that defines a layer contract, is not a violation.'
		true = 'A function exists whose whole body is one state read, one assignment, or one forwarded call, or that is named as a getter, setter, Is*, or Can* over independently accessible state'
		false = 'Every function body does real multi-statement work or is a serialization or codec adapter, or no function is defined'
	}
	rule51 = [ordered]@{
		instructions = 'Does `code` split a function call''s arguments or a declaration''s parameters across more than one line (other than a lambda or brace-initializer argument), or wrap an if, while, assignment, or return Boolean expression across lines when that expression joined onto one line would be 140 columns or fewer? A split of a Boolean expression that is longer than 140 columns is allowed. A single very long line is never a violation.'
		true = 'Arguments or parameters are wrapped across lines, or a Boolean expression that would fit in 140 columns is split across lines'
		false = 'Every call and declaration keeps its arguments on one line, and any wrapped Boolean expression is longer than 140 columns'
	}
	rule62 = [ordered]@{
		instructions = 'Does `code` contain an if statement whose condition joins two or more independent guard conditions with || and whose body is a single exit statement (return, continue, or break), where each condition on its own should have been a separate if with its own exit? An || inside a Boolean assignment or a non-exit body, an && condition, and separate ifs with their own bodies are not violations.'
		true = 'One if packs several independent guards with || in front of a single return, continue, or break'
		false = 'No if with an || condition guards a single exit statement; guards are already one condition per if, or the || is used elsewhere'
	}
}

# Rule 56 is asked per identifier over the block's whole name list: one short name among forty lines is
# missed when the block is the state, and a name judged alone loses the naming convention its siblings show.
$script:NameInstructions = 'Is the C++ identifier `{0}` (one of the names listed in `identifiers`, all taken from the same code) an abbreviation: does it contain a shortened, truncated, or contracted word such as ctx, cmd, buf, cnt, msg, calc, dist, idx, tmp, prev, pos, or num in place of the complete word? Strip any leading Hungarian type prefix (p, r, i, ui, b, f, e, c, m, s, g, k, vec, mat, f2, f3, f4, rvec) before judging. NOT abbreviations: the loop counters i, j, k; the iterator name it or a name ending in It; a type name ending in _t; established acronyms Crc, Ui, Gpu, Cpu, Uuid, Id, Ns, Ms, Us; and ordinary whole English words.'
$script:NameCriteria = [ordered]@{ true = 'The name contains a shortened form of a word where the complete word was expected'; false = 'The name is made of complete words, allowed prefixes, counters, iterators, or established acronyms' }
# Keywords, fixed-width types, namespaces, and attribute words; all-capital macros and names starting vk, Vk,
# VK_, XM, or k plus a capital are dropped by the patterns below instead.
$script:NameKeywords = @('if', 'else', 'for', 'while', 'return', 'continue', 'break', 'const', 'static', 'constexpr', 'inline', 'void', 'bool', 'float', 'double', 'int', 'char', 'auto', 'class', 'struct', 'public', 'private', 'protected', 'namespace', 'using', 'true', 'false', 'nullptr', 'this', 'new', 'delete', 'sizeof', 'switch', 'case', 'default', 'do', 'template', 'typename', 'thread_local', 'defined', 'endif', 'include', 'size_t', 'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t', 'std', 'common', 'engine', 'game', 'data', 'FXMVECTOR', 'unlikely', 'maybe_unused', 'restrict')

$result = [ordered]@{
	schemaVersion = 'broken-engine-style-rule-judgment/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Style-rule judgment did not run.'
	thresholds = [ordered]@{ block = $BlockThreshold; rule3 = $Rule3Threshold; name = $NameThreshold }
	inputTokens = 0
	blocks = @()
	measurement = $null
}

function Complete-StyleRuleJudgment([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = $Message
	$json = $result | ConvertTo-Json -Depth 16 -Compress
	if ([string]::IsNullOrWhiteSpace($OutputPath)) {
		$stream = [Console]::OpenStandardOutput()
		$bytes = $script:Utf8.GetBytes($json)
		$stream.Write($bytes, 0, $bytes.Length)
		$stream.Flush()
	}
	else {
		[IO.File]::WriteAllText($OutputPath, $json, $script:Utf8)
		"$Status $Code $Message ($($result.inputTokens) input tokens) -> $OutputPath"
	}
	exit $ExitCode
}

function Get-BlockIdentifiers([string] $Code) {
	# Comments, string literals, and names after `.`, `->`, or `::` are stripped first: those are members and
	# functions of other types, not names this block declares.
	$stripped = [regex]::Replace($Code, '//[^\n]*|"(?:\\.|[^"\\])*"|(?:\.|->|::)\s*[A-Za-z_]\w*', ' ')
	$names = [regex]::Matches($stripped, '\b[A-Za-z_][A-Za-z0-9_]*\b') | ForEach-Object { $_.Value } | Select-Object -Unique
	return @($names | Where-Object { $_.Length -ge 2 -and $script:NameKeywords -cnotcontains $_ -and $_ -cnotmatch '^(?:vk|Vk|VK_|XM|k[A-Z])' -and $_ -cnotmatch '^[A-Z_0-9]+$' })
}

if (-not (Test-Path -LiteralPath $CasesPath -PathType Leaf)) { Complete-StyleRuleJudgment 1 'error' 'cases.missing' "Cases file not found: '$CasesPath'." }
$cases = @(Get-Content -LiteralPath $CasesPath -Raw | ConvertFrom-Json -Depth 8)
if ($cases.Count -eq 0) { Complete-StyleRuleJudgment 1 'error' 'cases.empty' 'Cases file holds no cases.' }

# Named apart from the table it copies: variable names are case-insensitive, so `$blockQuestions` would be
# the table itself.
$blockQuestionSet = [ordered]@{}
foreach ($key in $script:BlockQuestions.Keys) {
	$question = $script:BlockQuestions[$key]
	$blockQuestionSet[$key] = [ordered]@{ type = 'noul'; instructions = $question.instructions; criteria = [ordered]@{ true = $question.true; false = $question.false } }
}

# Two requests per block, block questions then name questions, so response 2i and 2i+1 belong to block i.
$requests = [Collections.Generic.List[object]]::new()
$nameLists = [Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
	$code = ($case.code -join "`n")
	$requests.Add([ordered]@{ state = [ordered]@{ code = $code }; questions = $blockQuestionSet })
	$names = Get-BlockIdentifiers $code
	$nameLists.Add($names)
	$nameQuestions = [ordered]@{}
	foreach ($name in $names) {
		$nameQuestions[$name] = [ordered]@{ type = 'noul'; instructions = ($script:NameInstructions -f $name); criteria = $script:NameCriteria }
	}
	# A block with no name to judge still needs a request so the response pairing holds; Jev rejects an
	# empty question set, so it asks one placeholder question whose answer is discarded.
	if ($nameQuestions.Count -eq 0) { $nameQuestions['none'] = [ordered]@{ type = 'noul'; instructions = 'Is `identifiers` empty?' } }
	$requests.Add([ordered]@{ state = [ordered]@{ identifiers = $names }; questions = $nameQuestions })
}

$requestFile = [IO.Path]::GetTempFileName()
$responseFile = [IO.Path]::GetTempFileName()
try {
	[IO.File]::WriteAllText($requestFile, ($requests | ConvertTo-Json -Depth 16 -Compress), $script:Utf8)
	& (Join-Path $PSScriptRoot 'Invoke-Jev.ps1') -RequestPath $requestFile -OutputPath $responseFile | Out-Null
	$jevExit = $LASTEXITCODE
	$jev = Get-Content -LiteralPath $responseFile -Raw | ConvertFrom-Json -Depth 64
}
finally {
	Remove-Item -LiteralPath $requestFile, $responseFile -ErrorAction SilentlyContinue
}
if ($jev.status -eq 'blocked') { Complete-StyleRuleJudgment 2 'blocked' $jev.code "Jev did not run, so no block was judged: $($jev.message)" }
$result.inputTokens = [int64]$jev.inputTokens

$perRule = [ordered]@{}
foreach ($key in @($script:BlockQuestions.Keys) + @('rule56')) { $perRule[$key] = [ordered]@{ hit = 0; missed = 0; falseFlag = 0; clean = 0 } }
$blocks = @()
for ($i = 0; $i -lt $cases.Count; $i++) {
	$case = $cases[$i]
	$blockResponse = $jev.responses[2 * $i]
	$nameResponse = $jev.responses[2 * $i + 1]
	if ($null -ne $blockResponse.error -or $null -ne $nameResponse.error) {
		$blocks += [ordered]@{ id = $case.id; error = "$($blockResponse.error) $($nameResponse.error)".Trim() }
		continue
	}
	$probabilities = [ordered]@{}
	$flagged = @()
	foreach ($key in $script:BlockQuestions.Keys) {
		$p = [Math]::Round([double]$blockResponse.answers.$key.noul, 3)
		$probabilities[$key] = $p
		$threshold = if ($key -eq 'rule3') { $Rule3Threshold } else { $BlockThreshold }
		if ($p -ge $threshold) { $flagged += [ordered]@{ rule = $key; probability = $p } }
	}
	$names = @()
	foreach ($name in $nameLists[$i]) { $names += [ordered]@{ name = $name; probability = [Math]::Round([double]$nameResponse.answers.$name.noul, 3) } }
	$names = @($names | Sort-Object -Property { $_['probability'] } -Descending)
	$flaggedNames = @($names | Where-Object { $_['probability'] -ge $NameThreshold })
	if ($flaggedNames.Count -gt 0) { $flagged += [ordered]@{ rule = 'rule56'; probability = $flaggedNames[0].probability; names = @($flaggedNames | ForEach-Object { $_['name'] }) } }
	$flagged = @($flagged | Sort-Object -Property { $_['probability'] } -Descending)
	$expected = @($case.expected)
	$got = @($flagged | ForEach-Object { $_['rule'] })
	foreach ($key in $perRule.Keys) {
		$wanted = $expected -contains $key
		$fired = $got -contains $key
		if ($wanted -and $fired) { $perRule[$key].hit++ } elseif ($wanted) { $perRule[$key].missed++ } elseif ($fired) { $perRule[$key].falseFlag++ } else { $perRule[$key].clean++ }
	}
	# A label for a rule this script does not ask (rule 61, the scanner's) is not a miss.
	$missed = @($expected | Where-Object { $perRule.Contains($_) -and $got -notcontains $_ })
	$falseFlags = @($got | Where-Object { $expected -notcontains $_ })
	$blocks += [ordered]@{ id = $case.id; expected = $expected; missed = $missed; falseFlags = $falseFlags; flagged = $flagged; probabilities = $probabilities; names = $names }
}
$result.blocks = $blocks
$result.measurement = $perRule
if ($jevExit -ne 0) { Complete-StyleRuleJudgment 1 'error' 'jev.partial' 'Some requests failed; their blocks carry an error instead of probabilities.' }
$flaggedBlocks = @($blocks | Where-Object { $_.Contains('flagged') -and $_['flagged'].Count -gt 0 }).Count
Complete-StyleRuleJudgment 0 'ok' 'blocks.judged' "$($blocks.Count) blocks judged; $flaggedBlocks flagged for a hand read."
