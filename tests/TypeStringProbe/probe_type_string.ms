-- M0 probe for TYPE_STRING_TAB.
-- Run with: 3dsmaxbatch.exe tests\TypeStringProbe\probe_type_string.ms -p tests\TypeStringProbe\probe_plugin3.ini -dm on -log tests\TypeStringProbe\probe_type_string3.log -v 4

resultPath = "D:/PycharmProjects/CADDimension/tests/TypeStringProbe/probe_type_string_result.log"
out = createFile resultPath
fn wlog msg = (
    format "%\n" msg to:out
    flush out
)
fn check cond msg = (
    if cond then wlog ("[TSP][PASS] " + msg)
    else wlog ("[TSP][FAIL] " + msg)
)

wlog "[TSP][START]"

try (
    resetMaxFile #noPrompt
    p = TypeStringProbe()
    check (p != undefined) "class created"
    wlog ("[TSP][CLASS] " + (classOf p as string))
    wlog ("[TSP][NUMREFS] " + (refs.getNumRefs p.baseObject as string))
    check ((refs.getNumRefs p.baseObject) == 1) "NumRefs == 1"
    wlog ("[TSP][PROPS] " + (getPropNames p.baseObject as string))
    wlog ("[TSP][CPP] " + (p.cppReadback as string))
    check ((p.cppReadback as string) == "CPP:cpp0|n=4") "C++ wrote 4 cells; first == cpp0"
    wlog ("[TSP][CELLS] " + (p.cells as string))

    try ( wlog ("[TSP][CELL1] " + (p.cells[1] as string)) ) catch ( wlog ("[TSP][CELL1][ERR] " + (getCurrentException())) )
    try ( wlog ("[TSP][COUNT] " + (p.cells.count as string)) ) catch ( wlog ("[TSP][COUNT][ERR] " + (getCurrentException())) )

    try (
        p.cells[1] = "MS1"
        wlog ("[TSP][WRITE] cpp=" + (p.cppReadback as string))
        check ((p.cppReadback as string) == "CPP:MS1|n=4") "MAXScript write visible to C++ GetValue"
    ) catch ( wlog ("[TSP][WRITE][ERR] " + (getCurrentException())) )

    try (
        p2 = copy p
        wlog ("[TSP][CLONE] cpp=" + (p2.cppReadback as string))
        check ((p2.cppReadback as string) == "CPP:MS1|n=4") "clone preserves TYPE_STRING_TAB"
    ) catch ( wlog ("[TSP][CLONE][ERR] " + (getCurrentException())) )

    try (
        nm = p.name as string
        saveMaxFile "D:/PycharmProjects/CADDimension/tests/TypeStringProbe/probe_type_string.max"
        resetMaxFile #noPrompt
        loadMaxFile "D:/PycharmProjects/CADDimension/tests/TypeStringProbe/probe_type_string.max"
        p3 = getNodeByName nm
        if p3 == undefined then
            wlog ("[TSP][LOAD][ERR] node not found: " + nm)
        else (
            wlog ("[TSP][LOAD] cpp=" + (p3.cppReadback as string))
            check ((p3.cppReadback as string) == "CPP:MS1|n=4") "save/load preserves TYPE_STRING_TAB"
        )
    ) catch ( wlog ("[TSP][LOAD][ERR] " + (getCurrentException())) )
) catch (
    wlog ("[TSP][FATAL] " + (getCurrentException()))
)

close out
