-- M0 baseline: current Release plugin loads and CADDimension resolves.
resultPath = "D:/PycharmProjects/CADDimension/scripts/fixtures/current_load_result.log"
out = createFile resultPath
fn wlog msg = (
    format "%\n" msg to:out
    flush out
)

wlog "[CUR][START]"
try (
    resetMaxFile #noPrompt
    p = CADDimension()
    if p == undefined then
        wlog "[CUR][FAIL] CADDimension() returned undefined"
    else (
        wlog ("[CUR][CLASS] " + (classOf p as string))
        wlog ("[CUR][NUMREFS] " + (refs.getNumRefs p.baseObject as string))
        wlog "[CUR][PASS] current Release plugin loaded"
    )
) catch (
    wlog ("[CUR][FATAL] " + (getCurrentException()))
)
close out
