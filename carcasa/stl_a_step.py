"""Convierte los STL de la carcasa a STEP para abrirlos en SolidWorks u otro CAD.

El STEP resultante es de caras planas (triángulos): sirve para medir, montar
y añadir operaciones, pero para cambiar la forma conviene editar radio.scad.

Uso:  python stl_a_step.py stl/*.stl      (necesita: pip install cadquery-ocp)
"""
import pathlib
import sys

from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeSolid, BRepBuilderAPI_Sewing
from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPControl import STEPControl_AsIs, STEPControl_Writer
from OCP.StlAPI import StlAPI_Reader
from OCP.TopAbs import TopAbs_SHELL
from OCP.TopExp import TopExp_Explorer
from OCP.TopoDS import TopoDS, TopoDS_Shape


def convertir(stl: pathlib.Path) -> pathlib.Path:
    malla = TopoDS_Shape()
    StlAPI_Reader().Read(malla, str(stl))
    # cose los triángulos en una piel cerrada y la convierte en sólido
    cosido = BRepBuilderAPI_Sewing(1e-3)
    cosido.Add(malla)
    cosido.Perform()
    forma = cosido.SewedShape()
    solido = BRepBuilderAPI_MakeSolid()
    exp = TopExp_Explorer(forma, TopAbs_SHELL)
    while exp.More():
        solido.Add(TopoDS.Shell(exp.Current()))
        exp.Next()
    escritor = STEPControl_Writer()
    escritor.Transfer(solido.Solid(), STEPControl_AsIs)
    salida = stl.with_suffix(".step")
    assert escritor.Write(str(salida)) == IFSelect_RetDone, f"no se pudo escribir {salida}"
    return salida


if __name__ == "__main__":
    for nombre in sys.argv[1:]:
        print(convertir(pathlib.Path(nombre)))
