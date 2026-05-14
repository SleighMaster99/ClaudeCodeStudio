Set fso = CreateObject("Scripting.FileSystemObject")
Set sh  = CreateObject("WScript.Shell")
baseDir = fso.GetParentFolderName(WScript.ScriptFullName)
cmd = "cmd.exe /c powershell.exe -NoProfile -ExecutionPolicy Bypass -Sta -File """ & baseDir & "\StatusBarConfig.ps1"""
sh.Run cmd, 0, False
