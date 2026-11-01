// Comprueba que la placa no choca con el cuerpo: si el resultado está
// vacío ("top level object is empty"), no hay choque.
//   openscad -o choque.stl -D 'placa="cyd"' comprobar_choque.scad
include <radio.scad>
module dibujar() { intersection() { frente(); placa_referencia(); } }
