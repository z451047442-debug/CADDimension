-- M2 probe: CADLeader params, clone, save/load.
resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/cadleader_result.log"
out = createFile resultPath
fn wlog msg = ( format "%\n" msg to:out; flush out )
fn check cond msg = ( if cond then wlog ("[LEADER][PASS] " + msg) else wlog ("[LEADER][FAIL] " + msg) )

wlog "[LEADER][START]"
try (
    resetMaxFile #noPrompt
    l = CADLeader()
    check (l != undefined) "class created"
    wlog ("[LEADER][CLASS] " + (classOf l as string))
    l.start = [0, 0, 0]
    l.elbow = [10, 0, 10]
    l.textPos = [20, 0, 20]
    l.text = "Leader"
    l.textSize = 14
    l.arrowSize = 8
    check (l.elbow == [10, 0, 10]) "elbow persisted"
    check (l.textPos == [20, 0, 20]) "textPos persisted"
    check (l.text == "Leader") "text persisted"
    check (l.textSize == 14) "textSize persisted"
    check (l.arrowSize == 8) "arrowSize persisted"

    l2 = copy l
    check (l2.elbow == [10, 0, 10]) "clone preserves elbow"
    check (l2.text == "Leader") "clone preserves text"

    nm = l.name as string
    saveMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/cadleader.max"
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/cadleader.max"
    l3 = getNodeByName nm
    if l3 == undefined then
        wlog ("[LEADER][FAIL] save/load node not found: " + nm)
    else (
        check (l3.elbow == [10, 0, 10]) "save/load preserves elbow"
        check (l3.text == "Leader") "save/load preserves text"
        check (l3.arrowSize == 8) "save/load preserves arrowSize"
    )
) catch (
    wlog ("[LEADER][FATAL] " + (getCurrentException()))
)
wlog ("[LEADER][DONE] leader probe completed")
close out
