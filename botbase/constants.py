BUTTONS = {"A","B","X","Y","L","R","ZL","ZR","PLUS","MINUS","HOME","CAPTURE",
           "DUP","DDOWN","DLEFT","DRIGHT","LSTICK","RSTICK","PALMA","UNUSED"}
ALIAS = {"UP":"DUP","DOWN":"DDOWN","LEFT":"DLEFT","RIGHT":"DRIGHT",
         "START":"PLUS","SELECT":"MINUS",
         "LS":"LSTICK","RS":"RSTICK","L3":"LSTICK","R3":"RSTICK"}
DEFAULT_PORT = 6000

US_LAYOUT = {
    **{chr(ord("a") + i): (4 + i, False) for i in range(26)},
    **{chr(ord("A") + i): (4 + i, True) for i in range(26)},
    " ": (44, False), "\n": (40, False),
    **{str(d): (30 + (d-1) % 10, False) for d in range(10)},
    "!": (30, True), "@": (31, True), "#": (32, True), "$": (33, True),
    "%": (34, True), "^": (35, True), "&": (36, True), "*": (37, True),
    "(": (38, True), ")": (39, True),
    "-": (45, False), "=": (46, False), "_": (45, True), "+": (46, True),
    ".": (55, False), ",": (54, False), "?": (56, True), "/": (56, False),
}

CONTROLLER_TYPES = {
    "joycon-right": 1, "joycon-left": 2, "pro": 3,
    "pokeball": 12,
}
