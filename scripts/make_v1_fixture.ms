-- P0: generate a 1.0.0 three-point fixture with the current build.
resetMaxFile #noPrompt
d = CADDimension()
d.name = "V1Dim"
d.point1 = [0, 0, 0]
d.point2 = [100, 0, 0]
d.dimPoint = [50, 0, 50]
d.showXYZ = true
out = "D:/PycharmProjects/CADDimension/scripts/fixtures/v1_0_0_three_point.max"
saveMaxFile out
format "[V1][DONE] %\n" out
