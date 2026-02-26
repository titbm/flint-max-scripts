utility expUtil "Export Flint"
(
-- Base directory: folder where this script lives
-- getSourceFileName() requires Max 2013+; use execute() to avoid compile error in Max 7
local scriptDir = ""
try ( scriptDir = getFilenamePath (execute "getSourceFileName()" ) ) catch ()

local iniFile = (getDir #plugcfg) + "\\FlintExp.ini"
if scriptDir == "" do (
	local savedPath = getINISetting iniFile "Settings" "ScriptDir"
	if savedPath != "" do scriptDir = savedPath
)

-- Fallback to typical Scripts folder if not found yet
if scriptDir == "" do (
	if doesFileExist ((getDir #scripts) + "\\cftsort.exe") do scriptDir = (getDir #scripts) + "\\"
)

-- Detect .NET support (available since Max 9 / 2007)
local hasDotNet = false
try ( dotNetClass "System.Object"; hasDotNet = true ) catch ()

-- 16-bit EXE cannot run on 64-bit Windows (no NTVDM)
-- Max 7 is always 32-bit, so 16-bit sort is available there
local is64bit = false
if hasDotNet do (
	try ( is64bit = ((dotNetClass "System.Environment").GetEnvironmentVariable "PROCESSOR_ARCHITECTURE") == "AMD64" ) catch ()
)

global masterXform = (matrix3 1)

label lab2 "Coordinate System"
pickbutton chooseit "Current Coord System"
button cftSort "Sort 16-bit" enabled:(not is64bit)
button cftSort2 "Sort 32-bit"

button ImpoprtBut "Import CFT..."

-- Export scene geometry to CFT file
fn exportCFT dir name csName =
(
	SetWaitCursor()

	local masterTM = if csName == "Current Coord System" then (matrix3 1) else masterXform

	local ScalFil = 10

	deleteFile (dir + name + ".cft")

	local cftexp = createfile (dir + name + ".cft")
	local Shapka = "flinttab\nnglass=	1.506\nxsize=	50\nysize=	50\nzsize=	50\n" as string
	format Shapka to: cftexp
	for Nobj = 1 to geometry.count do
	(
		local Doubl = off
		local iGeom = geometry[Nobj]
		if (not iGeom.isFrozen and not iGeom.isHidden) do
		(
			try iGeom.numverts catch \
			(iGeom = convertToMesh (copy iGeom); Doubl = on)

			for Nvert = 1 to iGeom.numverts do
			( in coordsys masterTM (
				local iVert = getvert iGeom Nvert
				iVert = iVert * 100 / ScalFil
				local xVert = iVert.x as integer
				local yVert = -iVert.y as integer
				local zVert = (iVert.z / 1.506) as integer
				format "1:\t%\t%\t%\n" xVert yVert zVert to: cftexp
			))
		)
		if Doubl then delete iGeom
	)
	close cftexp
	SetArrowCursor()
)

-- Run sorter EXE; uses .NET progress window on Max 9+, HiddenDOSCommand on Max 7
fn runSorter exePath cftPath bftPath =
(
	if hasDotNet then (
		local psi = dotNetObject "System.Diagnostics.ProcessStartInfo"
		psi.FileName = exePath
		psi.Arguments = "\"" + cftPath + "\" \"" + bftPath + "\" 14 10"
		psi.UseShellExecute = false
		psi.CreateNoWindow = true
		psi.RedirectStandardOutput = true

		-- Progress form
		local frm = dotNetObject "System.Windows.Forms.Form"
		frm.Text = "CFTSORT"
		frm.Width = 700
		frm.Height = 100
		frm.StartPosition = (dotNetClass "System.Windows.Forms.FormStartPosition").CenterScreen
		frm.FormBorderStyle = (dotNetClass "System.Windows.Forms.FormBorderStyle").FixedDialog
		frm.ControlBox = false

		local pb = dotNetObject "System.Windows.Forms.ProgressBar"
		pb.Minimum = 0
		pb.Maximum = 100
		pb.Value = 0
		pb.Dock = (dotNetClass "System.Windows.Forms.DockStyle").Top
		pb.Height = 23

		local lbl = dotNetObject "System.Windows.Forms.Label"
		lbl.Text = "Sorting points... 0%"
		lbl.Dock = (dotNetClass "System.Windows.Forms.DockStyle").Fill
		lbl.TextAlign = (dotNetClass "System.Drawing.ContentAlignment").MiddleCenter

		frm.Controls.Add lbl
		frm.Controls.Add pb
		frm.Show()

		local proc = (dotNetClass "System.Diagnostics.Process").Start psi

		while not proc.HasExited do
		(
			while proc.StandardOutput.Peek() != -1 do
			(
				local line = proc.StandardOutput.ReadLine()
				if (findString line "PROGRESS:") == 1 then
				(
					local pct = (substring line 10 -1) as integer
					if pct != undefined then
					(
						pb.Value = pct
						lbl.Text = "Sorting points... " + pct as string + "%"
					)
				)
			)
			(dotNetClass "System.Windows.Forms.Application").DoEvents()
			sleep 0.05
		)

		pb.Value = 100
		lbl.Text = "Sorting points... 100%"
		frm.Refresh()
		sleep 0.3
		frm.Close()
		local result = proc.ExitCode
		proc.Close()
		if result != 0 then
			messageBox "Sort failed!" title:"Error"
		else
			messageBox "Sort completed!" title:"CFTSORT"
	) else (
		-- Max 7 fallback: run via HiddenDOSCommand (no progress bar)
		SetWaitCursor()
		local cmd = "\"" + exePath + "\" \"" + cftPath + "\" \"" + bftPath + "\" 14 10"
		local result = HiddenDOSCommand cmd
		SetArrowCursor()
		if result == 0 then
			messageBox "Sort completed!" title:"CFTSORT"
		else
			messageBox "Sort failed!" title:"Error"
	)
)

-- Get base filename from current 3ds Max scene, or fall back to "fromviz"
fn getSceneName =
(
	local n = getFilenameFile maxFileName
	if n == "" then "fromviz" else n
)

global lastExportFolder = ""

-- Open folder browser dialog, return path with trailing backslash or undefined
fn pickOutputFolder =
(
	if hasDotNet then (
		local fbd = dotNetObject "System.Windows.Forms.FolderBrowserDialog"
		fbd.Description = "Select folder to save CFT and BFT files"
		fbd.SelectedPath = if lastExportFolder != "" then lastExportFolder else maxFilePath
		local dr = fbd.ShowDialog()
		if dr.Equals ((dotNetClass "System.Windows.Forms.DialogResult").OK) then
		(
			lastExportFolder = fbd.SelectedPath
			(fbd.SelectedPath + "\\")
		)
		else
			undefined
	) else (
		-- Max 7 fallback: use getSavePath
		local initDir = if lastExportFolder != "" then lastExportFolder else maxFilePath
		local path = getSavePath caption:"Select folder to save CFT and BFT files" initialDir:initDir
		if path != undefined then (
			lastExportFolder = path
			(path + "\\")
		) else
			undefined
	)
)

on chooseit picked obj do 
(

global masterXform = obj.transform 
chooseit.text = obj.name 
) 


on ImpoprtBut pressed do
(

	if chooseit.text == "Current Coord System" do 
			(
			masterXform = (matrix3 1) --getCPTM ()
			)

	ScalFil = 10
--	ScalFil = case scaletype.state of 
--	( 
--		1: 1 
--		2: 10 
--		3: 100 
--	) 


local cftPath = getOpenFileName \
		caption:"Select CFT file" \
		types:"CFT Files (*.cft)|*.cft|All Files (*.*)|*.*"
	if cftPath == undefined do return()

	myFile = openFile cftPath mode:"r"
	if myFile == undefined do ( messageBox "Cannot open file." title:"Error"; return() )

skipToNextLine myFile
skipToNextLine myFile
skipToNextLine myFile
skipToNextLine myFile
skipToNextLine myFile

Dino =mesh numverts:0 numfaces:0
Nvert = 0

while not(eof myFile) do
	(
	Nstring = readline myFile

	numSp = findString Nstring "\t"
	if numSp == undefined do continue
	Nstring = substring Nstring (numSp+1) Nstring.count

	numSp = findString Nstring "\t"
	if numSp == undefined do continue
	XX = (substring Nstring 1 (numSp-1)) as float

	Nstring = substring Nstring (numSp+1) Nstring.count
	numSp = findString Nstring "\t"
	if numSp == undefined do continue
	YY = (substring Nstring 1 (numSp-1)) as float

	Nstring = substring Nstring (numSp+1) Nstring.count
	ZZ = Nstring as float

	Coord = [XX, YY, ZZ] / 100 * [1,-1,1.506] * ScalFil

	Nvert += 1
	setnumverts Dino Nvert true

	in coordsys masterXform (setvert Dino Nvert Coord)

	)
close myFile
select Dino

)


on cftSort pressed do
(
	local exePath = scriptDir + "cftsort.exe"
	if scriptDir == "" or not (doesFileExist exePath) do (
		exePath = getOpenFileName caption:"Locate cftsort.exe" types:"EXE (*.exe)|*.exe"
		if exePath == undefined do return()
		scriptDir = getFilenamePath exePath
		setINISetting iniFile "Settings" "ScriptDir" scriptDir
	)
	local folder = pickOutputFolder()
	if folder != undefined do
	(
		local baseName = getSceneName()
		exportCFT folder baseName chooseit.text
		runSorter exePath (folder + baseName + ".cft") (folder + baseName + ".bft")
	)
)

on cftSort2 pressed do
(
	local exePath = scriptDir + "cftsort32.exe"
	if scriptDir == "" or not (doesFileExist exePath) do (
		exePath = getOpenFileName caption:"Locate cftsort32.exe" types:"EXE (*.exe)|*.exe"
		if exePath == undefined do return()
		scriptDir = getFilenamePath exePath
		setINISetting iniFile "Settings" "ScriptDir" scriptDir
	)
	local folder = pickOutputFolder()
	if folder != undefined do
	(
		local baseName = getSceneName()
		exportCFT folder baseName chooseit.text
		runSorter exePath (folder + baseName + ".cft") (folder + baseName + ".bft")
	)
)

)