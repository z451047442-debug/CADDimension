-- Release-package smoke: verifies that the four CAD classes instantiate from
-- the DLO loaded through the extracted package directory.
-- Usage:
--   3dsmaxbatch.exe <package>\smoke_release.ms -p <package>\smoke_plugin.ini -dm on -log <package>\smoke.log -v 4
-- Writes smoke_release_result.log next to this script.

src = getSourceFileName()
if src == undefined then src = (getDir #temp) + "smoke_release.ms"
outPath = (getFilenamePath src) + "smoke_release_result.log"
out = createFile outPath
fn wlog msg = (
    format "%\n" msg to:out
    flush out
    format "%\n" msg
)

wlog "[SMOKE][START]"
wlog ("script=" + src)
try (
    resetMaxFile #noPrompt
    d = CADDimension()
    t = CADText()
    l = CADLeader()
    tb = CADTable()
    if d == undefined or t == undefined or l == undefined or tb == undefined do
        throw "one or more classes returned undefined"
    clsA = (classOf d as string) + "," + (classOf t as string)
    clsB = (classOf l as string) + "," + (classOf tb as string)
    cls = clsA + "," + clsB
    wlog ("classes=" + cls)
    wlog ("[SMOKE][PASS] " + cls)
) catch (
    wlog ("[SMOKE][FAIL] " + (getCurrentException()))
)
close out
