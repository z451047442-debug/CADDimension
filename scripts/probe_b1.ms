-- B1 runtime probe: identity, reference contract, clone D4, delete/undo/redo.
-- Run headless:
--   3dsmaxbatch.exe scripts\probe_b1.ms -log scripts\probe_b1.log -v 4
-- This script does not touch the interactive user's scene (batch is a separate process).
-- NOTE: MAXScript does not allow `local` declarations at a script's top level.

fn pass msg = (
    format "[PROBE][PASS] %\n" msg
)

fn fail msg = (
    format "[PROBE][FAIL] %\n" msg
    throw msg
)

fn info msg = (
    format "[PROBE][INFO] %\n" msg
)

resetMaxFile #noPrompt
format "[PROBE] 3ds Max version: %\n" (maxVersion() as string)

-- 1) Plugin class available and bind works.
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

-- 2) Single clone keeps the original target (D4).
cd2 = copy cd
if (cd2.targetNode != b) do fail ("single clone: target changed to " + (cd2.targetNode as string))
pass "single clone: targetNode == original ProbeBox"

-- 3) Whole-scene clone keeps the original target (D4).
select #(b, cd)
newNodes = #()
maxOps.cloneNodes selection cloneType:#copy newNodes:&newNodes
clonedDim = undefined
for n in newNodes do (
    if (((classOf n) as string) == "CADDimension") do clonedDim = n
)
if (clonedDim == undefined) do fail "whole-scene clone: cloned CADDimension not found"
if (clonedDim.targetNode != b) do fail ("whole-scene clone: target changed to " + (clonedDim.targetNode as string))
pass "whole-scene clone: cloned dimension keeps original ProbeBox"

-- 4) Two dimensions can share the same target.
cd3 = copy cd
cd3.name = "ProbeDimShared"
cd3.targetNode = b
if (cd3.targetNode != b) do fail "shared target: second dimension bind failed"
if (cd.targetNode != b) do fail "shared target: first dimension bind changed"
pass "shared target: two dimensions bind the same ProbeBox"

-- 5) Save/load persistence.
tempMax = "D:/PycharmProjects/CADDimension/scripts/probe_b1_scene.max"
if (doesFileExist tempMax) do deleteFile tempMax
saveMaxFile tempMax
resetMaxFile #noPrompt
loadMaxFile tempMax
b2 = getNodeByName "ProbeBox"
if (b2 == undefined) do fail "save/load: ProbeBox missing after load"
dims = for n in objects where (((classOf n) as string) == "CADDimension") collect n
if (dims.count == 0) do fail "save/load: no CADDimension found after load"
for d in dims do (
    if (d.targetNode != b2) do fail ("save/load: " + d.name + " target mismatch: " + (d.targetNode as string))
)
pass ("save/load: " + (dims.count as string) + " dimension(s) persist and point to ProbeBox")

-- 6) Delete target, undo, redo: must not crash or re-enter.
dim0 = dims[1]
delete b2
if (isValidNode b2) do fail "delete: ProbeBox still valid"
pass "delete: target deleted without crash"

max undo
restored = getNodeByName "ProbeBox"
if (restored == undefined) do fail "undo: ProbeBox not restored"
pass "undo: target restored without crash"

-- Record the post-undo binding; do not fail here, because the reference was
-- intentionally cleared on REFMSG_TARGET_DELETED. This is evidence for B1.
if (isValidNode dim0) then (
    info ("after undo, " + dim0.name + ".targetNode = " + (dim0.targetNode as string))
) else (
    info "after undo, dimension node was also removed"
)

max redo
pass "redo: no crash"

-- Record the post-redo state as well.
finalDims = for n in objects where (((classOf n) as string) == "CADDimension") collect n
info ("after redo, dimension count = " + (finalDims.count as string))

format "[PROBE][DONE] B1 probes completed\n"
