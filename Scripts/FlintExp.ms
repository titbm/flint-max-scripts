utility expUtil "Export Flint"
(
-- Base directory: folder where this script lives
local scriptDir = getFilenamePath (getSourceFileName())

-- 16-bit EXE cannot run on 64-bit Windows (no NTVDM)
-- 3ds Max 2013+ is 64-bit only, so Sort 16-bit is always disabled
local is64bit = ((dotNetClass "System.Environment").GetEnvironmentVariable "PROCESSOR_ARCHITECTURE") == "AMD64"

-- Intermediate CFT file: temp folder
local cftDir = getDir #temp + "\\"
local cftName = "fromviz"

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
	openfile (dir + name + ".cft")
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
	Nstring = substring Nstring (numSp+1) Nstring.count
	
	numSp = findString Nstring "\t" 
	XX = (substring Nstring 1 (numSp-1)) as float
	
	Nstring = substring Nstring (numSp+1) Nstring.count
	numSp = findString Nstring "\t" 
	YY = (substring Nstring 1 (numSp-1)) as float
	
	Nstring = substring Nstring (numSp+1) Nstring.count
	ZZ = (Nstring) as float
	
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
local bftFile = getSaveFileName caption:"Save BFT file" filename:(cftDir + cftName + ".bft") types:"BFT Files (*.bft)|*.bft|"
if bftFile != undefined do
(
	exportCFT cftDir cftName chooseit.text
	parametr = (cftDir + cftName + ".cft ") + bftFile + " 14 10"
	ShellLaunch (scriptDir + "cftsort.exe") parametr
)
)

on cftSort2 pressed do
(
local bftFile = getSaveFileName caption:"Save BFT file" filename:(cftDir + cftName + ".bft") types:"BFT Files (*.bft)|*.bft|"
if bftFile != undefined do
(
	exportCFT cftDir cftName chooseit.text
	parametr = (cftDir + cftName + ".cft ") + bftFile + " 14 10"
	ShellLaunch (scriptDir + "cftsort32.exe") parametr
)
)

)