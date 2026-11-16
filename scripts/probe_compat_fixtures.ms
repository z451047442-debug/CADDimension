-- P0: verify B4d (NumRefs==1 bbox) and 1.0.0 three-point fixtures load.
resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/compat_fixtures_result.log"
out = createFile resultPath
fn wlog msg = ( format "%\n" msg to:out; flush out )
fn check cond msg = ( if cond then wlog ("[COMPAT][PASS] " + msg) else wlog ("[COMPAT][FAIL] " + msg) )

wlog "[COMPAT][START]"
try (
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/b4d_bbox_nrefs1.max"
    d = getNodeByName "B4dDim"
    b = getNodeByName "B4dBox"
    check (d != undefined) "B4d fixture node found"
    check (b != undefined) "B4d target found"
    check (d.mode == 0) "B4d mode == 0"
    check (d.targetNode == b) "B4d target preserved"
) catch (
    wlog ("[COMPAT][B4D_FATAL] " + (getCurrentException()))
)

try (
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/v1_0_0_three_point.max"
    v = getNodeByName "V1Dim"
    check (v != undefined) "1.0.0 fixture node found"
    check (v.mode == 1) "1.0.0 mode == 1"
    check (v.point2 == [100, 0, 0]) "1.0.0 point2 preserved"
    check (v.showXYZ == true) "1.0.0 showXYZ preserved"
) catch (
    wlog ("[COMPAT][V1_FATAL] " + (getCurrentException()))
)

close out
