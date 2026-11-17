-- Package verification: load the three compatibility fixtures and assert their
-- persisted state. Run with the package plugin ini:
--   3dsmaxbatch.exe verify_fixtures.ms -p smoke_plugin.ini -dm on -log verify.log -v 4
src = getSourceFileName()
base = if src != undefined then (getFilenamePath src) else ""
out = createFile (base + "verify_fixtures_result.log")
fn wlog msg = ( format "%\n" msg to:out; flush out; format "%\n" msg )
fn check cond msg = (
    if cond then wlog ("[FIXTURES][PASS] " + msg)
    else wlog ("[FIXTURES][FAIL] " + msg)
)

wlog "[FIXTURES][START]"

-- 1) legacy B1 (NumRefs == 2)
try (
    resetMaxFile #noPrompt
    loadMaxFile (base + "fixtures/legacy_b1_nrefs2.max")
    d = getNodeByName "LegacyDim"
    b = getNodeByName "LegacyBox"
    check (d != undefined) "legacy CADDimension node found"
    check (b != undefined) "legacy target found"
    check (d.mode == 0) "legacy mode == 0"
    check (d.targetNode == b) "legacy target preserved"
) catch (
    wlog ("[FIXTURES][LEGACY_FATAL] " + (getCurrentException()))
)

-- 2) B4d bbox (NumRefs == 1)
try (
    resetMaxFile #noPrompt
    loadMaxFile (base + "fixtures/b4d_bbox_nrefs1.max")
    d = getNodeByName "B4dDim"
    b = getNodeByName "B4dBox"
    check (d != undefined) "B4d CADDimension node found"
    check (b != undefined) "B4d target found"
    check (d.mode == 0) "B4d mode == 0"
    check (d.targetNode == b) "B4d target preserved"
) catch (
    wlog ("[FIXTURES][B4D_FATAL] " + (getCurrentException()))
)

-- 3) 1.0.0 three-point
try (
    resetMaxFile #noPrompt
    loadMaxFile (base + "fixtures/v1_0_0_three_point.max")
    v = getNodeByName "V1Dim"
    check (v != undefined) "1.0.0 CADDimension node found"
    check (v.mode == 1) "1.0.0 mode == 1"
    check (v.point2 == [100, 0, 0]) "1.0.0 point2 preserved"
    check (v.showXYZ == true) "1.0.0 showXYZ preserved"
) catch (
    wlog ("[FIXTURES][V1_FATAL] " + (getCurrentException()))
)

wlog "[FIXTURES][DONE]"
close out
