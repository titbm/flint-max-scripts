utility expUtil "Export Flint"
(
-- Base directory: folder where this script lives
local scriptDir = getFilenamePath (getSourceFileName())

-- 16-bit EXE cannot run on 64-bit Windows (no NTVDM)
-- 3ds Max 2013+ is 64-bit only, so Sort 16-bit is always disabled
local is64bit = ((dotNetClass "System.Environment").GetEnvironmentVariable "PROCESSOR_ARCHITECTURE") == "AMD64"

global masterXform = (matrix3 1)

label lab2 "Coordinate System"
pickbutton chooseit "Current Coord System"
button cftSort "Sort 16-bit" enabled:(not is64bit)
button cftSort2 "Sort 32-bit"

local CFTab = getFiles (scriptDir + "*.cft")

 listbox   FileBox "Select File to import" items:CFTab
		height:3
 button ImpoprtBut "Import file"

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

-- Run sorter EXE with .NET progress window (marquee)
fn runSorter exePath cftPath bftPath =
(
	local psi = dotNetObject "System.Diagnostics.ProcessStartInfo"
	psi.FileName = exePath
	psi.Arguments = "\"" + cftPath + "\" \"" + bftPath + "\" 14 10"
	psi.UseShellExecute = false
	psi.CreateNoWindow = true

	-- Progress form
	local frm = dotNetObject "System.Windows.Forms.Form"
	frm.Text = "CFTSORT"
	frm.Width = 350
	frm.Height = 100
	frm.StartPosition = (dotNetClass "System.Windows.Forms.FormStartPosition").CenterScreen
	frm.FormBorderStyle = (dotNetClass "System.Windows.Forms.FormBorderStyle").FixedDialog
	frm.ControlBox = false

	local pb = dotNetObject "System.Windows.Forms.ProgressBar"
	pb.Style = (dotNetClass "System.Windows.Forms.ProgressBarStyle").Marquee
	pb.MarqueeAnimationSpeed = 30
	pb.Dock = (dotNetClass "System.Windows.Forms.DockStyle").Top
	pb.Height = 23

	local lbl = dotNetObject "System.Windows.Forms.Label"
	lbl.Text = "Sorting points... 0 sec"
	lbl.Dock = (dotNetClass "System.Windows.Forms.DockStyle").Fill
	lbl.TextAlign = (dotNetClass "System.Drawing.ContentAlignment").MiddleCenter

	frm.Controls.Add lbl
	frm.Controls.Add pb
	frm.Show()

	local proc = (dotNetClass "System.Diagnostics.Process").Start psi
	local elapsed = 0

	while not proc.HasExited do
	(
		sleep 0.3
		elapsed += 1
		lbl.Text = "Sorting points... " + (elapsed * 0.3 as integer) as string + " sec"
		frm.Refresh()
	)

	frm.Close()
	local result = proc.ExitCode
	proc.Close()
	if result != 0 then
		messageBox "Sort failed!" title:"Error"
	else
		messageBox "Sort completed!" title:"CFTSORT"
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


myFile = openFile FileBox.selected mode:"r"

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
	local folder = pickOutputFolder()
	if folder != undefined do
	(
		local baseName = getSceneName()
		exportCFT folder baseName chooseit.text
		runSorter (scriptDir + "cftsort.exe") (folder + baseName + ".cft") (folder + baseName + ".bft")
	)
)

on cftSort2 pressed do
(
	local folder = pickOutputFolder()
	if folder != undefined do
	(
		local baseName = getSceneName()
		exportCFT folder baseName chooseit.text
		runSorter (scriptDir + "cftsort32.exe") (folder + baseName + ".cft") (folder + baseName + ".bft")
	)
)

)