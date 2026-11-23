#!/usr/bin/env python3
"""Spanish for the text the 3DS port draws itself (control hints, its own menu lines, messages) and for the area
names on the save profiles: romfs/text/es/_port.txt, one 'English<TAB>Spanish' line each, Latin-1 like the sheets.
The game's own menus come from its MainMenu sheet (already in Spanish); these are the lines it doesn't have.
Area names are taken from the game's Map Zones sheet (English value -> Spanish value), plus the port's few others.
    python3 port_text.py <romfs/text>"""

import sys, os

PORT = [
    # title menu, profiles, options
    ("Clear save %d?", "¿Borrar la partida %d?"),
    ("B: back", "B: atrás"),
    ("Fan demake for personal use", "Demake de fans para uso personal"),
    (
        "A: play    X: clear save    B: back",
        "A: jugar    X: borrar partida    B: atrás",
    ),
    (
        "D-Pad: select    A: confirm    B: back",
        "Cruceta: elegir    A: aceptar    B: atrás",
    ),
    ("Left/Right: change    B: back", "Izq./Der.: cambiar    B: atrás"),
    ("Button Hints", "Ayuda de botones"),
    # in game
    ("Up: Enter", "Arriba: entrar"),
    (
        "B Jump  Y Nail  A Focus/Cast  R Dash  X Dream Nail",
        "B Saltar  Y Aguijón  A Concentrar/Hechizo  R Embestida  X Onírico",
    ),
    (
        "ZR Quick Cast  ZL Super Dash  SELECT Charms  START Pause",
        "ZR Hechizo rápido  ZL Superembestida  SELECT Amuletos  START Pausa",
    ),
    ("L / R: Charms", "L / R: Amuletos"),
    ("A: Unequip", "A: Quitar"),
    ("A: Equip", "A: Equipar"),
    ("L/R Charms   B/SELECT Close", "L/R Amuletos   B/SELECT Cerrar"),
    (
        "A Equip   L/R Items   B/SELECT Close",
        "A Equipar   L/R Objetos   B/SELECT Cerrar",
    ),
    ("A Buy   B Leave", "A Comprar   B Salir"),
    ("No map of this area", "No hay mapa de esta zona"),
    ("Journal entry complete", "Diario actualizado"),
    ("Hunter's Journal updated", "Nueva entrada en el diario"),
    ("Achievement unlocked", "Logro desbloqueado"),
    (
        "Charms can only be changed while resting at a bench.",
        "Los amuletos solo se pueden cambiar al descansar en un banco.",
    ),
    (
        "Mothwing Cloak - press R to dash",
        "Capa de ala de polilla: pulsa R para embestir",
    ),
    ("False Knight defeated", "Falso Caballero derrotado"),
    ("Geo recovered", "Geo recuperado"),
    ("Map updated", "Mapa actualizado"),
    ("Game saved", "Partida guardada"),
    ("Falsehood", "Falsedad"),
    ("Defeat the False Knight", "Derrota al Falso Caballero"),
    ("A  Begin the pantheon      B  Leave", "A  Comenzar el panteón      B  Salir"),
    ("A  Attuned challenge      B  Leave", "A  Desafío sintonizado      B  Salir"),
]
# the port's own area names (save profiles) that aren't a Map Zones value as written
ZONES = [("Hallownest", "Hallownest"), ("Fog Canyon", "Cañón de niebla")]


def sheet(path):
    d = {}
    for ln in open(path, "rb").read().decode("latin-1").split("\n"):
        if "\t" in ln:
            k, v = ln.split("\t", 1)
            d[k] = v.replace("\\n", " ").replace("\\f", " ").strip()
    return d


def main(text):
    en, es = sheet(f"{text}/Map Zones.txt"), sheet(f"{text}/es/Map Zones.txt")
    rows = list(PORT)
    seen = {a for a, b in rows}
    for k, v in en.items():
        if k in es and v not in seen and v and es[k]:
            rows.append((v, es[k]))
            seen.add(v)
    for a, b in ZONES:
        if a not in seen:
            rows.append((a, b))
            seen.add(a)
    out = "\n".join(f"{a}\t{b}" for a, b in rows) + "\n"
    open(f"{text}/es/_port.txt", "wb").write(out.encode("latin-1"))
    print(len(rows), "lines")


if __name__ == "__main__":
    main(sys.argv[1])
