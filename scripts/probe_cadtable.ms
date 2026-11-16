-- M3 probe: CADTable defaults, TYPE_STRING_TAB cells, resize, clone, save/load.
resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/cadtable_result.log"
out = createFile resultPath
fn wlog msg = ( format "%\n" msg to:out; flush out )
fn check cond msg = ( if cond then wlog ("[TABLE][PASS] " + msg) else wlog ("[TABLE][FAIL] " + msg) )

wlog "[TABLE][START]"
try (
    resetMaxFile #noPrompt
    t = CADTable()
    check (t != undefined) "class created"
    wlog ("[TABLE][CLASS] " + (classOf t as string))
    check (t.rows == 20) "default rows == 20"
    check (t.cols == 20) "default cols == 20"
    check (t.cellWidth == 80) "default cellWidth == 80"
    check (t.cellHeight == 24) "default cellHeight == 24"
    check (t.textSize == 12) "default textSize == 12"
    check (t.cells.count == 400) "default cells count == 400"

    t.cells[1] = "A1"
    t.cells[2] = "B1"
    check (t.cells[1] == "A1") "cell 1 write/read"
    check (t.cells[2] == "B1") "cell 2 write/read"

    -- Resize to 2x2; overlapping cells must be preserved.
    t.rows = 2
    t.cols = 2
    check (t.cells.count == 4) "resize cells count == 4"
    check (t.cells[1] == "A1") "resize preserves cell 1"
    check (t.cells[2] == "B1") "resize preserves cell 2"

    t2 = copy t
    check (t2.cells.count == 4) "clone preserves cells count"
    check (t2.cells[1] == "A1") "clone preserves cell 1"

    nm = t.name as string
    saveMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/cadtable.max"
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/cadtable.max"
    t3 = getNodeByName nm
    if t3 == undefined then
        wlog ("[TABLE][FAIL] save/load node not found: " + nm)
    else (
        check (t3.rows == 2) "save/load preserves rows"
        check (t3.cols == 2) "save/load preserves cols"
        check (t3.cells.count == 4) "save/load preserves cells count"
        check (t3.cells[1] == "A1") "save/load preserves cell 1"
        check (t3.cells[2] == "B1") "save/load preserves cell 2"

        -- Conditionally allowed max: 40x20 = 800 cells.
        t3.rows = 40
        t3.cols = 20
        check (t3.cells.count == 800) "40x20 cells count == 800"
    )
) catch (
    wlog ("[TABLE][FATAL] " + (getCurrentException()))
)
close out
