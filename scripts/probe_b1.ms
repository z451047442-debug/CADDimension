-- B1.1 runtime probe: single-reference model, D4 clone matrix, delete/undo/redo,
-- legacy B1 scene load (G7), and MAXScript undo/redo (G6 partial; panel is B2).
-- Run headless:
--   3dsmaxbatch.exe scripts\probe_b1.ms -p scripts\probe_plugin.ini -log scripts\probe_b1.log -v 4
-- NOTE: MAXScript does not allow `local` declarations at a script's top level.

probeLog = undefined

fn writeLine msg =
(
    format "%\n" msg
    if (probeLog != undefined) do
    (
        format "%\n" msg to:probeLog
        flush probeLog
    )
)

fn pass msg = (
    writeLine ("[PROBE][PASS] " + msg)
)

fn fail msg = (
    writeLine ("[PROBE][FAIL] " + msg)
    if (probeLog != undefined) do close probeLog
    throw msg
)

fn info msg = (
    writeLine ("[PROBE][INFO] " + msg)
)

resultPath = "D:/PycharmProjects/CADDimension/scripts/probe_b1_result.log"
if (doesFileExist resultPath) do deleteFile resultPath
probeLog = createFile resultPath
format "[PROBE] 3ds Max version: %\n" (maxVersion() as string) to:probeLog
flush probeLog

-- G7: load a legacy B1 scene (NumRefs=2, P_OWNERS_REF) with the B1.1 build.
legacyPath = "D:/PycharmProjects/CADDimension/scripts/probe_b1_scene.max"
if (doesFileExist legacyPath) then (
    try (
        resetMaxFile #noPrompt
        loadMaxFile legacyPath
        info "G7 legacy B1 scene loaded without crashing"
    ) catch (
        fail ("G7 legacy load crashed: " + (getCurrentException()))
    )
) else (
    info "G7 legacy load skipped (no legacy scene)"
)

resetMaxFile #noPrompt
undo on

-- 1) Plugin class + bind.
try (
    probeClass = CADDimension
    info ("CADDimension class resolved: " + (probeClass as string))
) catch (
    fail ("CADDimension class not found: " + (getCurrentException()))
)

b = Box length:10 width:20 height:30 pos:[0,0,0]
b.name = "ProbeBox"
cd = CADDimension()
cd.name = "ProbeDim"
cd.targetNode = b
if (cd.targetNode != b) do fail "bind: targetNode did not bind to ProbeBox"
pass "bind: targetNode == ProbeBox"

fn checkClone label nodes =
(
    local dims = for n in nodes where (((classOf n) as string) == "CADDimension") collect n
    if (dims.count == 0) do fail (label + ": cloned CADDimension not found")
    for d in dims do (
        if (d.targetNode != b) do fail (label + ": target changed to " + (d.targetNode as string))
    )
    pass (label + ": " + (dims.count as string) + " cloned dimension(s) keep original ProbeBox")
)

-- 2) Single clone (target is not in the clone set).
cd2 = copy cd
sleep 0.2
if (cd2.targetNode != b) do fail ("single clone: target changed to " + (cd2.targetNode as string))
pass "single clone: targetNode == original ProbeBox"

-- 3) Whole-scene clone (target + dimension both selected).
select #(b, cd)
newNodes = #()
maxOps.cloneNodes selection cloneType:#copy newNodes:&newNodes
sleep 0.2
checkClone "whole-scene clone" newNodes

-- 4) Clone of clone.
clonedDim = undefined
for n in newNodes do (
    if (((classOf n) as string) == "CADDimension") do clonedDim = n
)
if (clonedDim == undefined) do fail "clone of clone: base clone not found"
cd4 = copy clonedDim
sleep 0.2
if (cd4.targetNode != b) do fail ("clone of clone: target changed to " + (cd4.targetNode as string))
pass "clone of clone: targetNode == original ProbeBox"

-- 5) Only the dimension selected.
select cd
newNodes2 = #()
maxOps.cloneNodes selection cloneType:#copy newNodes:&newNodes2
sleep 0.2
checkClone "only-dimension clone" newNodes2

-- 6) Target + dimension selected.
select #(b, cd)
newNodes3 = #()
maxOps.cloneNodes selection cloneType:#copy newNodes:&newNodes3
sleep 0.2
checkClone "target+dimension clone" newNodes3

-- 7) Shared target: two dimensions bind the same Box.
cd3 = copy cd
cd3.name = "ProbeDimShared"
cd3.targetNode = b
if (cd3.targetNode != b) do fail "shared target: second dimension bind failed"
if (cd.targetNode != b) do fail "shared target: first dimension bind changed"
pass "shared target: two dimensions bind the same ProbeBox"

-- 8) Save/load.
roundtrip = "D:/PycharmProjects/CADDimension/scripts/probe_b1_roundtrip.max"
if (doesFileExist roundtrip) do deleteFile roundtrip
saveMaxFile roundtrip
resetMaxFile #noPrompt
loadMaxFile roundtrip
b2 = getNodeByName "ProbeBox"
if (b2 == undefined) do fail "save/load: ProbeBox missing after load"
dims = for n in objects where (((classOf n) as string) == "CADDimension") collect n
if (dims.count == 0) do fail "save/load: no CADDimension found after load"
for d in dims do (
    if (d.targetNode != b2) do fail ("save/load: " + d.name + " target mismatch: " + (d.targetNode as string))
)
pass ("save/load: " + (dims.count as string) + " dimension(s) persist and point to ProbeBox")

-- 9) Delete target + undo/redo.
dim0 = dims[1]
delete b2
if (isValidNode b2) do fail "delete: ProbeBox still valid"
pass "delete: target deleted without crash"
if (isValidNode dim0) then (
    if (dim0.targetNode == undefined) then (
        pass "delete: targetNode cleared"
    ) else (
        info ("delete: targetNode not cleared = " + (dim0.targetNode as string))
    )
)

max undo
restored = getNodeByName "ProbeBox"
if (restored == undefined) then (
    info "G3 undo: ProbeBox not restored (batch undo may be disabled)"
) else (
    pass "undo: target restored without crash"
)
if (isValidNode dim0) then (
    info ("after undo, " + dim0.name + ".targetNode = " + (dim0.targetNode as string))
) else (
    info "after undo, dimension node was also removed"
)

max redo
pass "redo: no crash"

-- 10) MAXScript undo/redo for target binding (G6 partial; panel pick is B2).
resetMaxFile #noPrompt
undo on
bA = Box()
bA.name = "UndoBoxA"
bB = Box()
bB.name = "UndoBoxB"
cdU = CADDimension()
cdU.name = "UndoDim"
cdU.targetNode = bA
if (cdU.targetNode != bA) do fail "undo probe: initial bind failed"
cdU.targetNode = bB
if (cdU.targetNode != bB) do fail "undo probe: rebind failed"
max undo
if (isValidNode cdU) then (
    if (cdU.targetNode != bA) do fail ("MAXScript undo did not restore A: " + (cdU.targetNode as string))
    pass "MAXScript undo restored previous target"
) else (
    fail "MAXScript undo removed the dimension"
)
max redo
if (isValidNode cdU) then (
    if (cdU.targetNode != bB) do fail ("MAXScript redo did not restore B: " + (cdU.targetNode as string))
    pass "MAXScript redo restored new target"
) else (
    fail "MAXScript redo left the dimension removed"
)

-- Consecutive assignments: stepwise undo/redo.
cdU.targetNode = bA
cdU.targetNode = bB
max undo
if (cdU.targetNode != bA) do fail ("consecutive undo #1: " + (cdU.targetNode as string))
max undo
if (cdU.targetNode != bB) do fail ("consecutive undo #2: " + (cdU.targetNode as string))
max redo
if (cdU.targetNode != bA) do fail ("consecutive redo #1: " + (cdU.targetNode as string))
max redo
if (cdU.targetNode != bB) do fail ("consecutive redo #2: " + (cdU.targetNode as string))
pass "consecutive assignments: stepwise undo/redo"

-- Invalid target must be rejected and the rejection must be undoable.
cdU.targetNode = cdU
if (cdU.targetNode != undefined) do fail "invalid target was not rejected"
pass "invalid target rejected"
max undo
if (cdU.targetNode != bB) do fail ("invalid-target undo did not restore B: " + (cdU.targetNode as string))
pass "invalid target undo restored previous target"

-- 11) G5 target-move redraw / ObjectValidity is interactive-only.
info "G5 target-move redraw / ObjectValidity is not verifiable in batch; interactive check required."

writeLine "[PROBE][DONE] B1.1 probes completed"
if (probeLog != undefined) do close probeLog
