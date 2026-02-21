utility expUtil "Export Flint"
(
-- Base directory: folder where this script lives
local scriptDir = getFilenamePath (getSourceFileName())

-- 16-bit EXE cannot run on 64-bit Windows (no NTVDM)
local is64bit = sysInfo.is64BitOS

label lab2 "Coordinate System"
pickbutton chooseit "Current Coord System"
edittext dirname "Dir: " text: scriptDir enabled:false 

edittext filename "Name: " text: "fromviz"
button cftSort "Sort 16-bit" enabled:(not is64bit)
button cftSort2 "Sort 32-bit"

local CFTab = getFiles (scriptDir + "*.cft")

 listbox   FileBox "Select File to import" items:CFTab
		height:3
 button ImpoprtBut "Import file"

-- Export scene geometry to CFT file
fn exportCFT dir name coordSys =
(
	SetWaitCursor()

	local masterTM = if coordSys == "Current Coord System" then (matrix3 1) else master

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

global master = obj.transform 
chooseit.text = obj.name 
) 


on ImpoprtBut pressed do
(

	if chooseit.text == "Current Coord System" do 
			(
			master = (matrix3 1) --getCPTM ()
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
	
	in coordsys master (setvert Dino Nvert Coord)

	)
close myFile
select Dino

)


on cftSort pressed do
(
local bftFile = getSaveFileName caption:"Save BFT file" filename:(dirname.text + filename.text + ".bft") types:"BFT Files (*.bft)|*.bft|"
if bftFile != undefined do
(
	exportCFT dirname.text filename.text chooseit.text
	parametr = (dirname.text + filename.text + ".cft ") + bftFile + " 14 10"
	ShellLaunch (scriptDir + "cftsort.exe") parametr
)
)

on cftSort2 pressed do
(
local bftFile = getSaveFileName caption:"Save BFT file" filename:(dirname.text + filename.text + ".bft") types:"BFT Files (*.bft)|*.bft|"
if bftFile != undefined do
(
	exportCFT dirname.text filename.text chooseit.text
	parametr = (dirname.text + filename.text + ".cft ") + bftFile + " 14 10"
	ShellLaunch (scriptDir + "cftsort32.exe") parametr
)
)

)