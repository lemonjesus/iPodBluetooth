'***************************************************************************
'             __________               __   ___.
'   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
'   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
'   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
'   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
'                     \/            \/     \/    \/            \/
' $Id$
'
' Copyright (C) 2007 Steve Bavin, Jens Arnold, Mesar Hameed
'
' All files in this archive are subject to the GNU General Public License.
' See the file COPYING in the source tree root for full license agreement.
'
' This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
' KIND, either express or implied.
'
'***************************************************************************

Option Explicit

Const SSFMCreateForWrite = 3

' Audio formats for SAPI5 filestream object
Const SPSF_8kHz16BitMono  = 6
Const SPSF_11kHz16BitMono = 10
Const SPSF_12kHz16BitMono = 14
Const SPSF_16kHz16BitMono = 18
Const SPSF_22kHz16BitMono = 22
Const SPSF_24kHz16BitMono = 26
Const SPSF_32kHz16BitMono = 30
Const SPSF_44kHz16BitMono = 34
Const SPSF_48kHz16BitMono = 38

Const STDIN  = 0
Const STDOUT = 1
Const STDERR = 2

Dim oShell, oArgs, oEnv
Dim oFSO, oStdIn, oStdOut
Dim bVerbose, bSAPI4, bList
Dim bMSSP
Dim sLanguage, sVoice, sSpeed, sName, sVendor

Dim oSpVoice, oSpFS ' SAPI5 voice and filestream
Dim oTTS, nMode ' SAPI4 TTS object, mode selector
Dim oVoice ' for traversing the list of voices
Dim nLangID, sSelectString

Dim aLine, aData    ' used in command reading

On Error Resume Next

Set oFSO = CreateObject("Scripting.FileSystemObject")
Set oStdIn = oFSO.GetStandardStream(STDIN, true)
Set oStdOut = oFSO.GetStandardStream(STDOUT, true)

Set oShell = CreateObject("WScript.Shell")
Set oEnv = oShell.Environment("Process")
bVerbose = (oEnv("V") <> "")

Set oArgs = WScript.Arguments.Named
bSAPI4 = oArgs.Exists("sapi4")
bMSSP = oArgs.Exists("mssp")
bList = oArgs.Exists("listvoices")
sLanguage = oArgs.Item("language")
sVoice = oArgs.Item("voice")
sSpeed = oArgs.Item("speed")


If bSAPI4 Then
    ' Create SAPI4 ActiveVoice object
    Set oTTS = WScript.CreateObject("ActiveVoice.ActiveVoice", "TTS_")
    If Err.Number <> 0 Then
        Err.Clear
        Set oTTS = WScript.CreateObject("ActiveVoice.ActiveVoice.1", "TTS_")
        If Err.Number <> 0 Then
            WScript.StdErr.WriteLine "Error - could not get ActiveVoice" _
                                     & " object. SAPI 4 not installed?"
            WScript.Quit 1
        End If
    End If
    oTTS.Initialized = 1

    If bList Then 
        ' Just list available voices for the selected language
        For Each nLangID in LangIDs(sLanguage)
            For nMode = 1 To oTTS.CountEngines
                If oTTS.LanguageID(nMode) = nLangID Then
                    WScript.StdErr.Write oTTS.ModeName(nMode) & ","
                End If
            Next
        Next
        WScript.StdErr.WriteLine
        WScript.Quit 0
    End If

    ' Select matching voice
    For Each nLangID in LangIDs(sLanguage)
        sSelectString = "LanguageID=" & nLangID
        If sVoice <> "" Then
            sSelectString = sSelectString & ";Speaker=" & sVoice _
                            & ";ModeName=" & sVoice
        End If
        nMode = oTTS.Find(sSelectString)
        If oTTS.LanguageID(nMode) = nLangID And (sVoice = "" Or _
           oTTS.Speaker(nMode) = sVoice Or oTTS.ModeName(nMode) = sVoice) Then
            sName = oTTS.ModeName(nMode)
            If bVerbose Then 
                WScript.StdErr.WriteLine "Using " & sName & " for " & sSelectString
            End If
            Exit For
        Else
            sSelectString = ""
        End If
    Next
    If sSelectString = "" Then
        WScript.StdErr.WriteLine "Error - found no matching voice for " _
                                 & sLanguage & ", " & sVoice
        WScript.Quit 1
    End If
    oTTS.Select nMode

    ' Speed selection
    If sSpeed <> "" Then oTTS.Speed = sSpeed

    ' Get vendor information
    sVendor = oTTS.MfgName(nMode)

Else ' SAPI5
    ' Create SAPI5 object
    If bMSSP Then
            Set oSpVoice = CreateObject("speech.SpVoice")
    Else
            Set oSpVoice = CreateObject("SAPI.SpVoice")
    End If
    If Err.Number <> 0 Then
        WScript.StdErr.WriteLine "Error " & Err.Number _
                                 & " - could not get SpVoice object." _
                                 & " SAPI 5 not installed?"
        WScript.Quit 1
    End If

    If bList Then
        ' Just list available voices for the selected language
        For Each nLangID in LangIDs(sLanguage)
            sSelectString = "Language=" & Hex(nLangID)
            For Each oVoice in oSpVoice.GetVoices(sSelectString)
                WScript.StdErr.Write oVoice.GetAttribute("Name") & ";"
            Next
        Next
        WScript.StdErr.WriteLine
        WScript.Quit 0
    End If

    ' Select matching voice
    For Each nLangID in LangIDs(sLanguage)
        sSelectString = "Language=" & Hex(nLangID)
        If sVoice <> "" Then
            sSelectString = sSelectString & ";Name=" & sVoice
        End If
        Set oSpVoice.Voice = oSpVoice.GetVoices(sSelectString).Item(0)
        If Err.Number = 0 Then
            sName = oSpVoice.Voice.GetAttribute("Name")
            If bVerbose Then
                WScript.StdErr.WriteLine "Using " & sName & " for " & sSelectString
            End If
            Exit For
        Else
            sSelectString = ""
            Err.Clear
        End If
    Next
    If sSelectString = "" Then
        WScript.StdErr.WriteLine "Error - found no matching voice for " _
                                 & sLanguage & ", " & sVoice
        WScript.Quit 1
    End If

    ' Speed selection
    If sSpeed <> "" Then oSpVoice.Rate = sSpeed

    ' Get vendor information, protect from missing attribute
    sVendor = oSpVoice.Voice.GetAttribute("Vendor")
    If Err.Number <> 0 Then
        Err.Clear
        sVendor = "(unknown)"
        ' Some L&H engines don't set the vendor attribute - check the name
        If Len(sName) > 3 And Left(sName, 3) = "LH " Then
            sVendor = "L&H"
        End If
    End If

    ' Filestream object for output
    Set oSpFS = CreateObject("SAPI.SpFileStream")
    oSpFS.Format.Type = AudioFormat(sVendor)
End If

Do
    aLine = Split(oStdIn.ReadLine, vbTab, 2)
    If Err.Number <> 0 Then
        WScript.StdErr.WriteLine "Error " & Err.Number & ": " & Err.Description
        WScript.Quit 1
    End If
    Select Case aLine(0) ' command
        Case "QUERY"
            Select Case aLine(1)
                Case "VENDOR"
                    oStdOut.WriteLine sVendor
            End Select
        Case "SPEAK"
            aData = Split(aLine(1), vbTab, 2)
            If bVerbose Then WScript.StdErr.WriteLine "Saying " & aData(1) _
                                                      & " in " & aData(0)
            If bSAPI4 Then
                oTTS.FileName = aData(0)
                oTTS.Speak aData(1)
                While oTTS.Speaking
                    WScript.Sleep 1
                Wend
                oTTS.FileName = ""
            Else
                oSpFS.Open aData(0), SSFMCreateForWrite, false
                Set oSpVoice.AudioOutputStream = oSpFS
                oSpVoice.Speak aData(1)
                oSpFS.Close
            End If
        Case "EXEC"
            If bVerbose Then WScript.StdErr.WriteLine "> " & aLine(1)
            oShell.Run aLine(1), 0, true
            If Err.Number <> 0 Then
                If Not bVerbose Then 
                    WScript.StdErr.Write "> " & aLine(1) & ": "
                End If
                If Err.Number = &H80070002 Then ' Actually file not found
                    WScript.StdErr.WriteLine "command not found"
                Else
                    WScript.StdErr.WriteLine "error " & Err.Number & ":" _
                                             & Err.Description
                End If
                WScript.Quit 2
            End If
        Case "SYNC"
            If bVerbose Then WScript.StdErr.WriteLine "Syncing"
            oStdOut.WriteLine aLine(1) ' Just echo what was passed
        Case "QUIT"
            If bVerbose Then WScript.StdErr.WriteLine "Quitting"
            WScript.Quit 0
    End Select
Loop

' Subroutines
' -----------

' SAPI5 output format selection based on engine
Function AudioFormat(ByRef sVendor)
    Select Case sVendor
        Case "Microsoft"
            AudioFormat = SPSF_22kHz16BitMono
        Case "AT&T Labs"
            AudioFormat = SPSF_32kHz16BitMono
        Case "Loquendo"
            AudioFormat = SPSF_16kHz16BitMono
        Case "ScanSoft, Inc"
            AudioFormat = SPSF_22kHz16BitMono
        Case "Voiceware"
            AudioFormat = SPSF_16kHz16BitMono
        Case Else
            AudioFormat = SPSF_22kHz16BitMono
            WScript.StdErr.WriteLine "Warning - unknown vendor """ & sVendor _
                                     & """ - using default wave format"
    End Select
End Function

' Language mapping rockbox->windows
Function LangIDs(ByRef sLanguage)
    Dim aIDs

    Select Case sLanguage
        Case "afrikaans"
            LangIDs = Array(&h436)
        Case "arabic"
            LangIDs = Array( &h401,  &h801,  &hc01, &h1001, &h1401, &h1801, _
                            &h1c01, &h2001, &h2401, &h2801, &h2c01, &h3001, _
                            &h3401, &h3801, &h3c01, &h4001)
            ' Saudi Arabia, Iraq, Egypt, Libya, Algeria, Morocco, Tunisia,
            ' Oman, Yemen, Syria, Jordan, Lebanon, Kuwait, U.A.E., Bahrain,
            ' Qatar
        Case "basque"
            LangIDs = Array(&h42d)
        Case "bulgarian"
            LangIDs = Array(&h402)
        Case "catala"
            LangIDs = Array(&h403)
        Case "chinese-simp"
            LangIDs = Array(&h804) ' PRC
        Case "chinese-trad"
            LangIDs = Array(&h404) ' Taiwan. Perhaps also Hong Kong, Singapore, Macau?
        Case "czech"
            LangIDs = Array(&h405)
        Case "dansk"
            LangIDs = Array(&h406)
        Case "deutsch"
            LangIDs = Array(&h407, &hc07, &h1007, &h1407) 
            ' Standard, Austrian, Luxembourg, Liechtenstein (Swiss -> wallisertitsch)
        Case "eesti"
            LangIDs = Array(&h425)
        Case "english-us"
            LangIDs = Array( &h409,  &h809,  &hc09, &h1009, &h1409, &h1809, _
                            &h1c09, &h2009, &h2409, &h2809, &h2c09, &h3009, _
                            &h3409)
            ' American, British, Australian, Canadian, New Zealand, Ireland,
            ' South Africa, Jamaika, Caribbean, Belize, Trinidad, Zimbabwe,
            ' Philippines
        Case "english"
            LangIDs = Array( &h809,  &h409,  &hc09, &h1009, &h1409, &h1809, _
                            &h1c09, &h2009, &h2409, &h2809, &h2c09, &h3009, _
                            &h3409)
            ' British, American, Australian, Canadian, New Zealand, Ireland,
            ' South Africa, Jamaika, Caribbean, Belize, Trinidad, Zimbabwe,
            ' Philippines
        Case "espanol"
            LangIDs = Array( &h40a,  &hc0a,  &h80a, &h100a, &h140a, &h180a, _
                            &h1c0a, &h200a, &h240a, &h280a, &h2c0a, &h300a, _
                            &h340a, &h380a, &h3c0a, &h400a, &h440a, &h480a, _
                            &h4c0a, &h500a)
            ' trad. sort., mordern sort., Mexican, Guatemala, Costa Rica,
            ' Panama, Dominican Republic, Venezuela, Colombia, Peru, Argentina,
            ' Ecuador, Chile, Uruguay, Paraguay, Bolivia, El Salvador,
            ' Honduras, Nicaragua, Puerto Rico
        Case "esperanto"
            WScript.StdErr.WriteLine "Error: no esperanto support in Windows"
            WScript.Quit 1
        Case "finnish"
            LangIDs = Array(&h40b)
        Case "francais"
            LangIDs = Array(&h40c, &hc0c, &h100c, &h140c, &h180c)
            ' Standard, Canadian, Swiss, Luxembourg, Monaco (Belgian -> walon)
        Case "galego"
            LangIDs = Array(&h456)
        Case "greek"
            LangIDs = Array(&h408)
        Case "hebrew"
            LangIDs = Array(&h40d)
        Case "hindi"
            LangIDs = Array(&h439)
        Case "hrvatski"
            LangIDs = Array(&h41a, &h101a) ' Croatia, Bosnia and Herzegovina
        Case "islenska"
            LangIDs = Array(&h40f)
        Case "italiano"
            LangIDs = Array(&h410, &h810) ' Standard, Swiss
        Case "japanese"
            LangIDs = Array(&h411)
        Case "korean"
            LangIDs = Array(&h412)
        Case "latviesu"
            LangIDs = Array(&h426)
        Case "lietuviu"
            LangIDs = Array(&h427)
        Case "magyar"
            LangIDs = Array(&h40e)
        Case "nederlands"
            LangIDs = Array(&h413, &h813) ' Standard, Belgian
        Case "norsk-nynorsk"
            LangIDs = Array(&h814)
        Case "norsk"
            LangIDs = Array(&h414) ' Bokmal
        Case "polski"
            LangIDs = Array(&h415)
        Case "portugues-brasileiro"
            LangIDs = Array(&h416)
        Case "portugues"
            LangIDs = Array(&h816)
        Case "romaneste"
            LangIDs = Array(&h418)
        Case "russian"
            LangIDs = Array(&h419)
        Case "slovak"
            LangIDs = Array(&h41B)
        Case "slovenscina"
            LangIDs = Array(&h424)
        Case "srpski"
            LangIDs = Array(&hc1a) ' Cyrillic
        Case "svenska"
            LangIDs = Array(&h41d, &h81d) ' Standard, Finland
        Case "tagalog"
            LangIDs = Array(&h464) ' Filipino, might not be 100% correct
        Case "thai"
            LangIDs = Array(&h41e)
        Case "turkce"
            LangIDs = Array(&h41f)
        Case "ukrainian"
            LangIDs = Array(&h422)
        Case "wallisertitsch"
            LangIDs = Array(&h807) ' Swiss German
        Case "walon"
            LangIDs = Array(&h80c) ' Belgian French
    End Select
End Function


