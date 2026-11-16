-- M0: creation undo semantics probe.
-- Expectation: object creation is one undoable operation; constructor pblock
-- writes must not create extra undo records.

resultPath = "D:/PycharmProjects/CADDimension/tests/TypeStringProbe/probe_create_undo_result.log"
out = createFile resultPath
fn wlog msg = (
    format "%\n" msg to:out
    flush out
)

wlog "[UNDO][START]"
resetMaxFile #noPrompt
undo on

p = TypeStringProbe()
if p == undefined then
    wlog "[UNDO][FAIL] object not created"
else (
    nm = p.name as string
    wlog ("[UNDO][CREATED] " + nm)

    max undo
    stillThere = isValidNode p
    if stillThere then
        wlog "[UNDO][FAIL] one undo did not remove the newly created object (extra undo records?)"
    else
        wlog "[UNDO][PASS] one undo removed the newly created object"

    max redo
    if isValidNode p then
        wlog "[UNDO][PASS] one redo restored the object"
    else
        wlog "[UNDO][FAIL] one redo did not restore the object"
)

close out
