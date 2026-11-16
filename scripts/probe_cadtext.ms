-- M2 probe: CADText params, clone, save/load.
resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/cadtext_result.log"
out = createFile resultPath
fn wlog msg = ( format "%\n" msg to:out; flush out )
fn check cond msg = ( if cond then wlog ("[TEXT][PASS] " + msg) else wlog ("[TEXT][FAIL] " + msg) )

wlog "[TEXT][START]"
try (
    resetMaxFile #noPrompt
    t = CADText()
    check (t != undefined) "class created"
    wlog ("[TEXT][CLASS] " + (classOf t as string))
    t.text = "Hello CAD"
    t.textSize = 24
    check (t.text == "Hello CAD") "text persisted"
    check (t.textSize == 24) "textSize persisted"

    t2 = copy t
    check (t2.text == "Hello CAD") "clone preserves text"
    check (t2.textSize == 24) "clone preserves textSize"

    nm = t.name as string
    saveMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/cadtext.max"
    resetMaxFile #noPrompt
    loadMaxFile "D:/PycharmProjects/CADDimension/scripts/fixtures/cadtext.max"
    t3 = getNodeByName nm
    if t3 == undefined then
        wlog ("[TEXT][FAIL] save/load node not found: " + nm)
    else (
        check (t3.text == "Hello CAD") "save/load preserves text"
        check (t3.textSize == 24) "save/load preserves textSize"
    )
) catch (
    wlog ("[TEXT][FATAL] " + (getCurrentException()))
)
close out
