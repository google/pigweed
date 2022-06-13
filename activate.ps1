# Invokes a Cmd.exe shell script and updates the environment.
function Invoke-CmdScript {
    param(
        [String] $scriptName
    )
    $cmdLine = """$scriptName"" $args & set"
    & $Env:SystemRoot\system32\cmd.exe /c $cmdLine |
            Select-String '^([^=]*)=(.*)$' |
            ForEach-Object {
                $varName = $_.Matches[0].Groups[1].Value
                $varValue = $_.Matches[0].Groups[2].Value
                Set-Item Env:$varName $varValue
            }
}

Invoke-CmdScript activate.bat