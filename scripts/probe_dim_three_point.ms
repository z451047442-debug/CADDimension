-- M1b probe: three-point parameters, p_mode, showXYZ, clone/save/load,
-- point undo, and legacy NumRefs==2 migration.
-- Run with: 3dsmaxbatch.exe scripts\probe_dim_three_point.ms -p scripts\probe_plugin.ini -dm on -log scripts\probe_dim_three_point.log -v 4

resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/dim_three_point_result.log"
out = createFile resultPath
fn wlog msg = (
    format "%\n" msg to:out
    flush out
)
fn check cond msg = (
    if cond then wlog ("[DIM][PASS] " + msg)
    else wlog ("[DIM][FAIL] " + msg)
)

wlog "[DIM][START]"

try (
    resetMaxFile #noPrompt
    d = CADDimension()
    check (d != undefined) "new object created"
    check (d.mode == 1) "new object mode == 1"

    d.point1 = [0, 0, 0]
    d.point2 = [100, 0, 0]
    d.dimPoint = [50, 0, 50]
    d.showXYZ = true
    d.textSize = 14
    d.arrowSize = 8
    d.extensionGap = 3

    check ((length (d.point2 - d.point1)) == 100.0) "point distance == 100"
    check (d.showXYZ == true) "showXYZ persisted on object"
    check (d.textSize == 14) "textSize == 14"
    check (d.arrowSize == 8) "arrowSize == 8"
    check (d.extensionGap == 3) "extensionGap == 3"

    d2 = copy d
    check (d2.point2 == d.point2) "clone preserves point2"
    check (d2.mode == 1) "clone preserves mode"

    nm = d.name as string
    saveMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/dim_three_point.max"
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/dim_three_point.max"
    d3 = getNodeByName nm
    if d3 == undefined then
        wlog ("[DIM][FAIL] save/load node not found: " + nm)
    else (
        check (d3.point2 == [100, 0, 0]) "save/load preserves point2"
        check (d3.showXYZ == true) "save/load preserves showXYZ"
        check (d3.mode == 1) "save/load preserves mode"

        undo on
        d3.point2 = [50, 0, 0]
        check (d3.point2 == [50, 0, 0]) "point2 assignment applied"
        max undo
        check (d3.point2 == [100, 0, 0]) "undo restores point2"
        max redo
        check (d3.point2 == [50, 0, 0]) "redo reapplies point2"
    )
) catch (
    wlog ("[DIM][FATAL] " + (getCurrentException()))
)

-- Legacy B1 scene: NumRefs==2, target in compatibility slot, mode defaults 0.
try (
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/legacy_b1_nrefs2.max"
    L = getNodeByName "LegacyDim"
    b = getNodeByName "LegacyBox"
    check (L != undefined) "legacy LegacyDim found"
    check (b != undefined) "legacy LegacyBox found"
    check (L.mode == 0) "legacy mode == 0"
    check (L.targetNode == b) "legacy target preserved after migration"

    n = refs.getNumRefs L.baseObject
    check (n == 2) "legacy NumRefs == 2"
    r2 = refs.getReference L.baseObject 2
    wlog ("[DIM][LEGACY_REF2] " + (r2 as string))
    check (r2 == undefined) "legacy compatibility slot cleared after migration"

    L2 = copy L
    check (L2.targetNode == b) "legacy clone preserves target"
    check (L2.mode == 0) "legacy clone preserves mode 0"

    -- Migrated scene: save/reload, then delete the target.
    saveMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/legacy_migrated.max"
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/legacy_migrated.max"
    L3 = getNodeByName "LegacyDim"
    b3 = getNodeByName "LegacyBox"
    check (L3 != undefined) "migrated scene LegacyDim found"
    check (b3 != undefined) "migrated scene LegacyBox found"
    check (L3.mode == 0) "migrated scene mode == 0"
    check (L3.targetNode == b3) "migrated scene target preserved"
    r3 = refs.getReference L3.baseObject 2
    check (r3 == undefined) "migrated scene slot 2 still cleared"

    delete b3
    check (L3.targetNode == undefined) "target deletion clears pblock target"
) catch (
    wlog ("[DIM][LEGACY_FATAL] " + (getCurrentException()))
)

close out
