// =====================================================================
//  Radio Mundial — carcasa de radio de sobremesa, estilo años 50–60
//  OpenSCAD paramétrico (probado con OpenSCAD 2021.01)
//
//  Piezas:
//    frente  -> cuerpo con frontal, paredes, postes de la placa y del
//               altavoz. Se imprime boca abajo (el frontal sobre la cama).
//    tapa    -> tapa trasera con 4 tornillos M3 y ranuras de ventilación.
//    aro     -> aro que sujeta el altavoz de 40 mm (3 tornillos M2,5).
//
//  Coordenadas: X a la derecha, Y hacia arriba, Z hacia quien mira.
//  El frontal está en z = 0 y la caja crece hacia z negativo.
//
//  Licencia: CC BY-NC-SA 4.0 — @Adriancubas1980
// =====================================================================

/* [Qué dibujar] */
// conjunto, explosion, frente, tapa, aro o todo_impresion
pieza = "conjunto";
// fnk0104 (ESP32-S3, la placa principal) o cyd (Freenove FNK0114F)
placa = "fnk0104";
// Mostrar la placa dentro del conjunto (la CYD con su modelo real)
ver_placa = true;
// Corte por la mitad para ver el interior (solo en conjunto)
corte = false;

/* [Caja] */
ancho = 150;        // X
alto = 74;          // Y
fondo = 45;         // Z, frontal + tapa incluidos
radio_esquina = 10;
pared = 2.5;        // paredes laterales
grosor_frente = 2.5;
grosor_tapa = 2.5;
chaflan = 1.2;      // chaflán del borde del frontal y de la tapa
piel_frente = 1.2;  // plástico mínimo entre un agujero guía y la cara frontal
holgura = 0.25;     // holgura de encaje entre piezas
guia_grosor = 2;    // pestaña de la tapa que entra en las paredes
guia_alto = 4;
guia_chaflan = 0.6;

/* [Inclinación] */
// Grados hacia atrás (0 = sin patas)
inclinacion = 8;
ancho_pata = 22;

/* [Altavoz] */
altavoz_d = 40;          // diámetro exterior del altavoz
altavoz_brida = 2.5;     // grosor del borde del altavoz
rejilla_d = 34;          // zona abierta de la rejilla
altavoz_cx = 120;        // centro de la rejilla
altavoz_cy = 35;
aro_tornillos_r = 25.5;  // radio de los 3 tornillos del aro
// ángulos de los tornillos del aro: ninguno bajo el texto del frontal
aro_angulos = [30, 150, 270];

/* [Textos (grabados, en inglés)] */
texto_frente = "RADIO MUNDIAL";
texto_tapa_1 = "RADIO MUNDIAL";
texto_tapa_2 = "USB-C 5V  ·  @Adriancubas1980";
fuente = "Liberation Sans:style=Bold";
grabado = 0.6;

$fn = 64;

// ---------------------------------------------------------------------
//  Datos de las placas (todas: PCB 86 × 50, agujeros Ø3,2 a 4 mm del borde)
//  Coordenadas de la placa: origen en la esquina inferior izquierda vista
//  de frente, con el USB-C a la IZQUIERDA (setRotation(1) del firmware)
//  y z = 0 en la cara delantera del PCB.
// ---------------------------------------------------------------------
pcb = [86, 50, 1.6];
agujeros = [[4, 4], [82, 4], [4, 46], [82, 46]];

// [pila delante del PCB, x del área activa, ancho área, alto área,
//  fondo de componentes detrás del PCB, z del centro del USB-C]
// CYD: medido en el STEP de Freenove (3D MODELS/).
// FNK0104: de la especificación ES3C28P (10,6 mm total, 4,7 detrás).
//          POR VERIFICAR con la placa en la mano (llega el 2026-11-03).
datos_cyd     = [2.83, 17.10, 57.6, 43.2, 6.33, -2.69];
datos_fnk0104 = [4.30, 11.12, 57.6, 43.2, 6.30, -2.69];
d = placa == "cyd" ? datos_cyd : datos_fnk0104;
pila    = d[0];
area_x  = d[1];
area_w  = d[2];
area_h  = d[3];
trasera = d[4];
usb_z   = d[5];

// Dónde va la placa dentro de la caja
placa_x = pared + 1.5;
placa_y = (alto - pcb[1]) / 2;
placa_z = -(grosor_frente + pila);   // cara delantera del PCB

margen_ventana = 0.4;   // por lado, sobre el área activa
ventana = [area_w + 2 * margen_ventana, area_h + 2 * margen_ventana];
ventana_c = [placa_x + area_x + area_w / 2, placa_y + pcb[1] / 2];

fondo_cuerpo = fondo - grosor_tapa;
esquinas = [[7, 7], [ancho - 7, 7], [7, alto - 7], [ancho - 7, alto - 7]];

// ---------------------------------------------------------------------
//  Formas básicas
// ---------------------------------------------------------------------
module contorno(o = 0) {
    offset(r = radio_esquina + o) offset(delta = -radio_esquina)
        square([ancho, alto]);
}

module rect_redondo(t, r) {
    offset(r = r) offset(delta = -r) translate(-t / 2) square(t);
}

// Prisma de contorno con chaflán en la cara z = 0
module bloque_chaflan(h) {
    hull() {
        translate([0, 0, -0.01]) linear_extrude(0.01) contorno(-chaflan);
        translate([0, 0, -h]) linear_extrude(h - chaflan) contorno();
    }
}

// ---------------------------------------------------------------------
//  Frente (cuerpo)
// ---------------------------------------------------------------------
module patas() {
    if (inclinacion > 0) {
        h_atras = 1;
        h_delante = h_atras + fondo_cuerpo * tan(inclinacion);
        for (x = [16, ancho - 16 - ancho_pata])
            hull() {
                translate([x, -h_delante, -1]) cube([ancho_pata, h_delante + 1, 1]);
                translate([x, -h_atras, -fondo_cuerpo]) cube([ancho_pata, h_atras + 1, 1]);
            }
    }
}

module postes_placa() {
    for (a = agujeros)
        translate([placa_x + a[0], placa_y + a[1], placa_z])
            cylinder(d = 5.5, h = pila + 0.01);
}

module torretas_esquinas() {
    for (e = esquinas)
        translate([e[0], e[1], -fondo_cuerpo]) cylinder(d = 7, h = fondo_cuerpo - 0.01);
}

module soporte_altavoz() {
    translate([altavoz_cx, altavoz_cy, -grosor_frente - altavoz_brida]) {
        // anillo que centra el altavoz
        difference() {
            cylinder(r = altavoz_d / 2 + 0.4 + 1.6, h = altavoz_brida + 0.01);
            translate([0, 0, -1]) cylinder(r = altavoz_d / 2 + 0.4, h = altavoz_brida + 2);
        }
        // 3 torretas para los tornillos del aro
        for (ang = aro_angulos)
            rotate(ang) translate([aro_tornillos_r, 0, 0]) cylinder(d = 6, h = altavoz_brida + 0.01);
    }
}

module rejilla() {
    // ranuras horizontales recortadas en círculo, como las radios de baquelita
    translate([altavoz_cx, altavoz_cy, -grosor_frente - 1])
        linear_extrude(grosor_frente + 2)
            intersection() {
                circle(d = rejilla_d);
                for (y = [-rejilla_d / 2 : 3.2 : rejilla_d / 2])
                    translate([-rejilla_d / 2, y - 0.9]) square([rejilla_d, 1.8]);
            }
}

module ventana_pantalla() {
    // con bisel a 45° hacia fuera
    bisel = 1.2;
    translate([ventana_c[0], ventana_c[1], 0]) hull() {
        translate([0, 0, -grosor_frente - 1]) linear_extrude(grosor_frente + 1 - bisel)
            rect_redondo(ventana, 1);
        translate([0, 0, 0.01]) linear_extrude(0.01) rect_redondo(ventana + [2, 2] * bisel, 1 + bisel);
    }
}

module grabados_frente() {
    translate([0, 0, -grabado]) linear_extrude(grabado + 1) {
        // marco alrededor de la pantalla
        translate(ventana_c) difference() {
            rect_redondo(ventana + [9, 9], 4);
            rect_redondo(ventana + [7.4, 7.4], 3.2);
        }
        // aro alrededor de la rejilla
        translate([altavoz_cx, altavoz_cy]) difference() {
            circle(d = rejilla_d + 6.4);
            circle(d = rejilla_d + 4.8);
        }
        // dos filetes "cromados" de lado a lado
        for (y = [5, alto - 5]) translate([radio_esquina + 2, y - 0.4]) square([ancho - 2 * radio_esquina - 4, 0.8]);
        // nombre
        translate([altavoz_cx, altavoz_cy + rejilla_d / 2 + 5.5])
            text(texto_frente, size = 3.8, font = fuente, halign = "center", spacing = 1.08);
    }
}

module hueco_usb() {
    // en la pared izquierda, delante del conector de la placa
    translate([-1, placa_y + pcb[1] / 2, placa_z + usb_z])
        rotate([0, 90, 0]) linear_extrude(pared + 2)
            rect_redondo([8, 13], 2.5);   // [z, y] tras el giro
}

module frente() {
    difference() {
        union() {
            difference() {
                bloque_chaflan(fondo_cuerpo);
                translate([0, 0, -fondo_cuerpo - 1])
                    linear_extrude(fondo_cuerpo - grosor_frente + 1) contorno(-pared);
            }
            intersection() {
                // lo interior no puede salirse del contorno
                translate([0, 0, -fondo_cuerpo]) linear_extrude(fondo_cuerpo) contorno(-0.5);
                union() { postes_placa(); torretas_esquinas(); soporte_altavoz(); }
            }
            patas();
        }
        ventana_pantalla();
        rejilla();
        grabados_frente();
        hueco_usb();
        // agujeros guía M3 de la placa (M3 × 6 en la FNK0104, M3 × 5 en la CYD)
        for (a = agujeros)
            translate([placa_x + a[0], placa_y + a[1], placa_z - 1]) cylinder(d = 2.5, h = pila + 1 + grosor_frente - piel_frente);
        // agujeros guía M3 de la tapa (tornillo M3 × 10)
        for (e = esquinas)
            translate([e[0], e[1], -fondo_cuerpo - 1]) cylinder(d = 2.6, h = 13);
        // agujeros guía M2,5 del aro del altavoz (M2,5 × 5)
        translate([altavoz_cx, altavoz_cy, -grosor_frente - altavoz_brida - 1])
            for (ang = aro_angulos)
                rotate(ang) translate([aro_tornillos_r, 0, 0]) cylinder(d = 2.1, h = altavoz_brida + 1 + grosor_frente - piel_frente);
    }
}

// ---------------------------------------------------------------------
//  Tapa trasera
// ---------------------------------------------------------------------
module tapa() {
    z0 = -fondo;
    difference() {
        union() {
            // placa con chaflán en la cara exterior
            translate([0, 0, z0]) mirror([0, 0, 1]) bloque_chaflan(grosor_tapa);
            // pestaña que encaja dentro de las paredes, con chaflán en la punta
            translate([0, 0, z0 + grosor_tapa - 0.01]) difference() {
                hull() {
                    linear_extrude(guia_alto - guia_chaflan) contorno(-pared - holgura);
                    linear_extrude(guia_alto) contorno(-pared - holgura - guia_chaflan);
                }
                translate([0, 0, -1]) linear_extrude(guia_alto + 2) contorno(-pared - holgura - guia_grosor);
                for (e = esquinas) translate([e[0], e[1], -1]) cylinder(d = 9, h = guia_alto + 2);
            }
        }
        // tornillos M3 avellanados
        for (e = esquinas) translate([e[0], e[1], z0 - 0.01]) {
            cylinder(d = 3.4, h = grosor_tapa + 1);
            cylinder(d1 = 6.4, d2 = 3.4, h = 1.6);
        }
        // ventilación: ranuras horizontales centradas detrás de la placa
        // (rect_redondo dibuja centrado; lejos de la pestaña)
        translate([placa_x + pcb[0] / 2, placa_y + pcb[1] / 2, z0 - 1]) linear_extrude(grosor_tapa + 2)
            for (i = [-3 : 3]) translate([0, i * 5]) rect_redondo([64, 2.4], 1.1);
        // textos grabados (se leen mirando la tapa por detrás)
        translate([0, 0, z0 - 1]) linear_extrude(grabado + 1) mirror([1, 0, 0]) {
            translate([-(ancho - 38), altavoz_cy + 4]) text(texto_tapa_1, size = 4.5, font = fuente, halign = "center", spacing = 1.08);
            translate([-(ancho - 38), altavoz_cy - 6]) text(texto_tapa_2, size = 2.6, font = fuente, halign = "center");
        }
    }
}

// ---------------------------------------------------------------------
//  Aro del altavoz
// ---------------------------------------------------------------------
module aro() {
    // recortado para que no toque las paredes
    intersection() {
        translate([0, 0, -fondo]) linear_extrude(fondo) contorno(-pared - 0.5);
        translate([altavoz_cx, altavoz_cy, -grosor_frente - altavoz_brida - 2])
            difference() {
                cylinder(r = aro_tornillos_r + 3.5, h = 2);
                translate([0, 0, -1]) cylinder(d = altavoz_d - 4, h = 4);
                for (ang = aro_angulos)
                    rotate(ang) translate([aro_tornillos_r, 0, -1]) cylinder(d = 2.9, h = 4);
            }
    }
}

// ---------------------------------------------------------------------
//  Placa y altavoz de referencia (solo para ver el montaje)
// ---------------------------------------------------------------------
module placa_referencia() {
    translate([placa_x, placa_y, placa_z])
        if (placa == "cyd") {
            // STEP de Freenove convertido a STL; en el STEP el USB está a la derecha
            color("#2E7D4F") translate([1.79, 77.68, 0]) rotate(180)
                import("referencia/cyd_fnk0114f.stl");
        } else {
            // FNK0104 aproximada: PCB, pantalla y zona de componentes
            color("#1B5E3A") translate([0, 0, -pcb[2]]) difference() {
                cube(pcb);
                for (a = agujeros) translate([a[0], a[1], -1]) cylinder(d = 3.2, h = 4);
            }
            color("#111") translate([area_x - 6, (pcb[1] - area_h) / 2 - 3, 0]) cube([area_w + 12, area_h + 6, pila]);
            color("#3A3A3A") translate([10, 6, -trasera]) cube([70, 38, trasera - pcb[2]]);
            color("#C0C0C0") translate([-0.4, pcb[1] / 2 - 4.5, usb_z - 1.6]) cube([7.5, 9, 3.2]);
        }
}

module altavoz_referencia() {
    color("#555") translate([altavoz_cx, altavoz_cy, -grosor_frente - altavoz_brida]) {
        cylinder(d = altavoz_d, h = altavoz_brida);
        translate([0, 0, -14]) cylinder(d = 24, h = 14);
    }
}

// ---------------------------------------------------------------------
//  Qué dibujar
// ---------------------------------------------------------------------
color_cuerpo = "#EADBC0";   // marfil
color_tapa   = "#7A4E2D";   // nogal
color_aro    = "#C9A227";   // latón

module conjunto(sep = 0) {
    color(color_cuerpo) frente();
    translate([0, 0, -sep]) color(color_tapa) tapa();
    translate([0, 0, -sep / 2]) color(color_aro) aro();
    if (ver_placa) {
        translate([0, 0, -sep / 4]) placa_referencia();
        translate([0, 0, -sep / 2]) altavoz_referencia();
    }
}

module dibujar() {
if (pieza == "conjunto") {
    if (corte)
        difference() { conjunto(); translate([-1, alto / 2, -fondo - 20]) cube([ancho + 2, alto, fondo + 40]); }
    else conjunto();
}
else if (pieza == "explosion") conjunto(45);
// piezas colocadas para imprimir (z = 0 en la cama)
else if (pieza == "frente") rotate([180, 0, 0]) frente();
else if (pieza == "tapa") translate([0, 0, fondo]) tapa();
else if (pieza == "aro") translate([0, 0, grosor_frente + altavoz_brida + 2]) aro();
else if (pieza == "todo_impresion") {
    rotate([180, 0, 0]) frente();
    translate([0, -alto - 10, fondo]) tapa();
    translate([0, -2 * alto - 10, grosor_frente + altavoz_brida + 2]) aro();
}
}

// de_pie = true pone la radio derecha (Y hacia arriba pasa a Z) para las vistas
de_pie = false;
if (de_pie) rotate([90, 0, 0]) dibujar(); else dibujar();
