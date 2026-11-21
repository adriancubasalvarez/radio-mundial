"""
Vistas en PNG de la mochila para revisarla sin abrir un programa de CAD
(render con VTK, que viene con CadQuery):
  img/mochila_interior.png     la mochila por dentro, vacía
  img/mochila_con_bateria.png  igual, con la batería (azul) y el DAC (morado)
  img/conjunto.png             frontal + mochila montados
  img/mochila_fondo.png        la mochila por fuera (rejilla, cajeados, patas)
  img/jack.png                 el lateral derecho, con el agujero del jack

Uso:  python vistas.py
"""
import pathlib

import cadquery as cq
import vtk

import mochila_dac as m

AQUI = pathlib.Path(__file__).resolve().parent
IMG = AQUI / "img"


def actor(wp, color, opacidad=1.0):
    datos = wp.val().toVtkPolyData(0.1, 0.2)
    normales = vtk.vtkPolyDataNormals()
    normales.SetInputData(datos)
    normales.SetFeatureAngle(35)
    mapa = vtk.vtkPolyDataMapper()
    mapa.SetInputConnection(normales.GetOutputPort())
    a = vtk.vtkActor()
    a.SetMapper(mapa)
    r, g, b = (int(color[i:i + 2], 16) / 255 for i in (1, 3, 5))
    a.GetProperty().SetColor(r, g, b)
    a.GetProperty().SetOpacity(opacidad)
    a.GetProperty().SetSpecular(0.15)
    return a


def foto(piezas, camara, nombre, arriba=(0, 0, 1), ancho=1100, alto=760):
    ren = vtk.vtkRenderer()
    ren.SetBackground(1, 1, 1)
    for wp, color in piezas:
        ren.AddActor(actor(wp, color))
    ven = vtk.vtkRenderWindow()
    ven.SetOffScreenRendering(1)
    ven.AddRenderer(ren)
    ven.SetSize(ancho, alto)
    cam = ren.GetActiveCamera()
    cam.SetFocalPoint(0, 0, 0)
    cam.SetPosition(*camara)
    cam.SetViewUp(*arriba)
    ren.ResetCamera()
    cam.Zoom(1.15)
    ven.Render()
    captura = vtk.vtkWindowToImageFilter()
    captura.SetInput(ven)
    captura.Update()
    IMG.mkdir(exist_ok=True)
    png = vtk.vtkPNGWriter()
    png.SetFileName(str(IMG / nombre))
    png.SetInputConnection(captura.GetOutputPort())
    png.Write()
    print("img/" + nombre)


if __name__ == "__main__":
    mochila = m.mochila()
    frontal = cq.importers.importStep(str(AQUI.parent / "pequena" / "front.step"))
    bateria = (cq.Workplane("YZ", origin=(m.X_SUELO - m.BAT_H, m.BAT_Y, m.BAT_Z))
               .rect(m.BAT_L, m.BAT_A).extrude(m.BAT_H))
    cy = m.dac_centro_y()
    dac = cq.Workplane("YZ", origin=(m.X_SUELO - 7, cy, m.DAC_Z)).rect(m.DAC_A, m.DAC_L).extrude(7)

    AMARILLO, OCRE = "#f2c230", "#d9a400"
    # por dentro: cámara del lado del frontal (-X), algo por arriba
    foto([(mochila, AMARILLO)], (-200, -60, 90), "mochila_interior.png")
    foto([(mochila, AMARILLO), (bateria, "#3a7bd5"), (dac, "#8e44ad")], (-200, -60, 90),
         "mochila_con_bateria.png")
    foto([(frontal, AMARILLO), (mochila, OCRE)], (160, -170, 90), "conjunto.png")
    foto([(mochila, AMARILLO)], (220, 60, -80), "mochila_fondo.png")
    foto([(mochila, AMARILLO)], (40, 220, 10), "jack.png")
