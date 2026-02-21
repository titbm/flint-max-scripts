fn delta oper =
(
    local rezult
    in coordsys screen
    (
        oper.x = abs oper.x
        oper.y = abs (oper.y+0.1)
        if oper.x > oper.y then rezult = oper.x else rezult = oper.y
        if oper.z > 0 then rezult = rezult + (oper.z*2) else rezult = rezult + oper.z
    )
    rezult
)

SetWaitCursor()
for sett=1 to selection.count do
(
    in coordsys screen
    (
        myMesh = selection[sett]
        Nverts = myMesh.numverts
    )
    in coordsys myMesh.transform
    (
        for i=2 to Nverts do
        (
            P1 = getVert myMesh 1
            P2 = getVert myMesh i
            if P1.z > P2.z do
            (
                setVert myMesh 1 P2
                setVert myMesh i P1
            )
        )
    )
    progressStart "Execution:"
    for i=2 to (Nverts-1) do
    (
        P1 = getVert myMesh (i-1)
        for j=(i+1) to Nverts do
        (
            P2 = getVert myMesh i
            P3 = getVert myMesh j
            DP2 = delta (P2 - P1)
            DP3 = delta (P3 - P1)
            if (DP3 < DP2) do
            (
                setVert myMesh i P3
                setVert myMesh j P2
            )
        )
        if getProgressCancel() do exit
        progressUpdate ((i*100/Nverts) as integer)
    )
    progressEnd()
    update myMesh
)
setArrowCursor()
