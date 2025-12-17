#!/usr/bin/env python3
"""
Generate data/rpg/loot.json from the Equipment sprite sheets.

This keeps the loot table data-driven and avoids hand-editing hundreds of entries.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT_PATH = ROOT / "data" / "rpg" / "loot.json"


def slugify(text: str) -> str:
    s = text.lower()
    s = s.replace("&", "and")
    s = re.sub(r"[^\w\s-]", "", s)
    s = re.sub(r"[\s-]+", "_", s).strip("_")
    return s


SLOT = {
    "Head": 0,
    "Chest": 1,
    "Legs": 2,
    "Boots": 3,
    "MainHand": 4,
    "OffHand": 5,
    "Ring1": 6,
    "Ring2": 7,
    "Amulet": 8,
    "Trinket": 9,
    "Cloak": 10,
    "Gloves": 11,
    "Belt": 12,
    "Ammo": 13,
    "Bag": 14,
    "Consumable": 15,
}

RARITY = {"Common": 0, "Uncommon": 1, "Rare": 2, "Epic": 3, "Legendary": 4}


def stats(flat: dict | None = None, mult: dict | None = None) -> dict:
    out: dict = {}
    if flat:
        out["flat"] = flat
    if mult:
        out["mult"] = mult
    return out


def flat_resists(**kwargs: float) -> dict:
    return {"resists": kwargs}


AFFIXES = [
    {"id": "atk_flat_s", "name": "+3 Attack Power", "stats": stats(flat={"attackPower": 3})},
    {"id": "atk_flat_m", "name": "+6 Attack Power", "stats": stats(flat={"attackPower": 6})},
    {"id": "spell_flat_s", "name": "+4 Spell Power", "stats": stats(flat={"spellPower": 4})},
    {"id": "spell_flat_m", "name": "+8 Spell Power", "stats": stats(flat={"spellPower": 8})},
    {"id": "armor_flat_s", "name": "+1 Armor", "stats": stats(flat={"armor": 1})},
    {"id": "armor_flat_m", "name": "+2 Armor", "stats": stats(flat={"armor": 2})},
    {"id": "hp_flat_s", "name": "+12 Max Health", "stats": stats(flat={"healthMax": 12})},
    {"id": "hp_flat_m", "name": "+25 Max Health", "stats": stats(flat={"healthMax": 25})},
    {"id": "shield_flat_s", "name": "+12 Max Shields", "stats": stats(flat={"shieldMax": 12})},
    {"id": "shield_flat_m", "name": "+25 Max Shields", "stats": stats(flat={"shieldMax": 25})},
    {"id": "as_mult_s", "name": "+6% Attack Speed", "stats": stats(mult={"attackSpeed": 0.06})},
    {"id": "ms_mult_s", "name": "+5% Move Speed", "stats": stats(mult={"moveSpeed": 0.05})},
    {"id": "crit_flat_s", "name": "+3% Crit Chance", "stats": stats(flat={"critChance": 0.03})},
    {"id": "crit_flat_m", "name": "+6% Crit Chance", "stats": stats(flat={"critChance": 0.06})},
    {"id": "ten_flat_s", "name": "+4 Tenacity", "stats": stats(flat={"tenacity": 4})},
    {"id": "eva_flat_s", "name": "+3 Evasion", "stats": stats(flat={"evasion": 3})},
    {"id": "arp_flat_s", "name": "+3 Armor Pen", "stats": stats(flat={"armorPen": 3})},
    {"id": "cdr_flat_s", "name": "+3% Cooldown Reduction", "stats": stats(flat={"cooldownReduction": 0.03})},
    {"id": "gold_flat_s", "name": "+5% Gold Gain", "stats": stats(flat={"goldGainMult": 0.05})},
    {"id": "rarity_flat_s", "name": "+0.6 Rarity Score", "stats": stats(flat={"rarityScore": 0.6})},
    {"id": "fire_res_s", "name": "+8% Fire Resist", "stats": stats(flat=flat_resists(Fire=0.08))},
    {"id": "frost_res_s", "name": "+8% Frost Resist", "stats": stats(flat=flat_resists(Frost=0.08))},
    {"id": "shock_res_s", "name": "+8% Shock Resist", "stats": stats(flat=flat_resists(Shock=0.08))},
    {"id": "poison_res_s", "name": "+8% Poison Resist", "stats": stats(flat=flat_resists(Poison=0.08))},
    {"id": "arcane_res_s", "name": "+8% Arcane Resist", "stats": stats(flat=flat_resists(Arcane=0.08))},
]


ARMOR_TIER = {
    "Copper": ("Common", 1.0),
    "Iron": ("Uncommon", 2.0),
    "Silver": ("Rare", 3.0),
    "Gold": ("Epic", 4.0),
    "Admantium": ("Epic", 5.0),
    "Legendary": ("Legendary", 6.0),
}


def armor_base(piece: str, tier_name: str) -> tuple[str, dict, list[str], int]:
    rarity, tier = ARMOR_TIER[tier_name]
    # piece multipliers
    mult = {
        "Head": 1.0,
        "Chest": 1.8,
        "Legs": 1.4,
        "Boots": 0.8,
        "Gloves": 0.6,
        "Belt": 0.7,
        "Cloak": 0.9,
    }[piece]
    base_armor = round(tier * mult, 2)
    flat = {"armor": base_armor}
    if piece == "Boots":
        return rarity, stats(flat=flat, mult={"moveSpeed": 0.03 + (tier - 1.0) * 0.005}), ["ms_mult_s", "eva_flat_s", "armor_flat_s"], 2
    if piece == "Gloves":
        return rarity, stats(flat=flat, mult={"attackSpeed": 0.04 + (tier - 1.0) * 0.006}), ["as_mult_s", "atk_flat_s", "armor_flat_s"], 2
    if piece == "Belt":
        return rarity, stats(flat={**flat, "healthMax": int(10 + tier * 6)}), ["hp_flat_s", "armor_flat_s", "ten_flat_s"], 2
    if piece == "Cloak":
        return rarity, stats(flat={**flat, "evasion": 2 + int(tier)}, mult={"moveSpeed": 0.02 + (tier - 1.0) * 0.004}), ["eva_flat_s", "ms_mult_s", "fire_res_s", "frost_res_s"], 2
    # Default armor pieces
    return rarity, stats(flat={**flat, "healthMax": int(6 + tier * 5)}), ["armor_flat_s", "hp_flat_s", "ten_flat_s", "fire_res_s"], 2


def weapon_base(kind: str, tier: str) -> tuple[str, dict, list[str], int]:
    # kind: melee/ranged -> attackPower, magic -> spellPower
    if tier == "Common":
        base = 3
    elif tier == "Uncommon":
        base = 5
    elif tier == "Rare":
        base = 8
    elif tier == "Epic":
        base = 12
    else:
        base = 16
    if kind == "magic":
        return tier, stats(flat={"spellPower": base}), ["spell_flat_s", "spell_flat_m", "cdr_flat_s", "arcane_res_s"], RARITY[tier] + 1
    return tier, stats(flat={"attackPower": base}), ["atk_flat_s", "atk_flat_m", "as_mult_s", "crit_flat_s", "arp_flat_s"], RARITY[tier] + 1


def offhand_base(kind: str, tier: str) -> tuple[str, dict, list[str], int]:
    # shield/quiver/tome
    if kind == "shield":
        base_armor = {"Common": 2, "Uncommon": 3, "Rare": 4, "Epic": 6, "Legendary": 8}[tier]
        base_shield = {"Common": 10, "Uncommon": 16, "Rare": 22, "Epic": 30, "Legendary": 40}[tier]
        return tier, stats(flat={"armor": base_armor, "shieldMax": base_shield}), ["armor_flat_s", "armor_flat_m", "shield_flat_s", "ten_flat_s"], RARITY[tier] + 1
    if kind == "quiver":
        base_acc = {"Common": 2, "Uncommon": 4, "Rare": 6, "Epic": 8, "Legendary": 10}[tier]
        return tier, stats(flat={"accuracy": base_acc}, mult={"attackSpeed": 0.05 + 0.02 * RARITY[tier]}), ["as_mult_s", "crit_flat_s", "arp_flat_s", "ms_mult_s"], RARITY[tier] + 1
    # tome
    base_sp = {"Common": 3, "Uncommon": 5, "Rare": 8, "Epic": 12, "Legendary": 16}[tier]
    return tier, stats(flat={"spellPower": base_sp, "cooldownReduction": 0.02 + 0.01 * RARITY[tier]}), ["spell_flat_s", "spell_flat_m", "cdr_flat_s", "arcane_res_s"], RARITY[tier] + 1


def accessory_base(kind: str, tier: str) -> tuple[str, dict, list[str], int]:
    # ring/amulet/trinket/ammo
    if kind == "ammo":
        base_ap = {"Common": 1, "Uncommon": 2, "Rare": 3, "Epic": 4, "Legendary": 6}[tier]
        return tier, stats(flat={"attackPower": base_ap, "armorPen": 1 + RARITY[tier]}), ["atk_flat_s", "crit_flat_s", "arp_flat_s"], RARITY[tier] + 1
    if kind == "ring":
        return tier, stats(flat={"critChance": 0.01 + 0.01 * RARITY[tier]}), ["crit_flat_s", "crit_flat_m", "atk_flat_s", "spell_flat_s", "rarity_flat_s"], RARITY[tier] + 1
    if kind == "amulet":
        return tier, stats(flat={"healthMax": 10 + 10 * RARITY[tier], "tenacity": 2 + 2 * RARITY[tier]}), ["hp_flat_s", "hp_flat_m", "ten_flat_s", "fire_res_s", "frost_res_s"], RARITY[tier] + 1
    # trinket
    return tier, stats(flat={"rarityScore": 0.25 + 0.25 * RARITY[tier]}), ["rarity_flat_s", "gold_flat_s", "cdr_flat_s", "arcane_res_s"], RARITY[tier] + 1


def max_affixes_for_rarity(tier: str, suggested: int) -> int:
    return max(1, min(suggested, 1 + RARITY[tier]))


def sockets_for(slot: str, rarity: str) -> int:
    # Gems are socketable items and don't themselves have sockets.
    if slot in {"Ring1", "Ring2", "Amulet", "Trinket", "Ammo"}:
        return 0
    r = RARITY[rarity]
    if r <= RARITY["Uncommon"]:
        return 1 if slot in {"MainHand", "OffHand"} else 0
    if r == RARITY["Rare"]:
        return 1
    if r == RARITY["Epic"]:
        return 2
    return 2


def add_item(items: list[dict], name: str, slot: str, rarity: str, icon_sheet: str, row: int, col: int, base: dict,
             pool: list[str], max_affixes: int, *, socketable: bool = False, sockets_max: int = 0,
             consumable_id: str | None = None, bag_slots: int = 0) -> None:
    item_id = slugify(name)
    payload = {
        "id": item_id,
        "name": name,
        "slot": SLOT[slot],
        "rarity": RARITY[rarity],
        "iconSheet": icon_sheet,
        "iconRow": row,
        "iconCol": col,
        "socketable": bool(socketable),
        "socketsMax": int(sockets_max),
        "stats": base,
        "maxAffixes": int(max_affixes),
        "affixPool": pool,
    }
    if consumable_id:
        payload["consumableId"] = consumable_id
    if bag_slots > 0:
        payload["bagSlots"] = int(bag_slots)
    items.append(payload)


def main() -> None:
    items: list[dict] = []

    # Gear.png (12x6)
    # Row 1: rings + amulets
    ring_names = ["Green Ring", "Blue Ring", "Orange Ring", "Purple Ring", "Grey Ring", "Yellow Ring"]
    ring_rarity = {"Grey Ring": "Common", "Green Ring": "Uncommon", "Blue Ring": "Rare", "Yellow Ring": "Rare", "Orange Ring": "Epic", "Purple Ring": "Legendary"}
    for c, nm in enumerate(ring_names):
        tier, base, pool, max_aff = accessory_base("ring", ring_rarity[nm])
        add_item(items, nm, "Ring1", tier, "Gear.png", 0, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for("Ring1", tier))

    amulet_names = ["Green Amulet", "Iron Amulet", "Basic Amulet", "Diamond Amulet", "Heart Amulet", "Purple Amulet"]
    amulet_rarity = {"Green Amulet": "Common", "Iron Amulet": "Uncommon", "Basic Amulet": "Common", "Diamond Amulet": "Epic", "Heart Amulet": "Rare", "Purple Amulet": "Legendary"}
    for c, nm in enumerate(amulet_names, start=6):
        tier, base, pool, max_aff = accessory_base("amulet", amulet_rarity[nm])
        add_item(items, nm, "Amulet", tier, "Gear.png", 0, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for("Amulet", tier))

    # Row 2: chest (M) + capes
    row = 1
    armor_pairs = [
        ("Copper Chest (M)", "Chest", "Copper"),
        ("Copper Cape", "Cloak", "Copper"),
        ("Iron Chest (M)", "Chest", "Iron"),
        ("Iron Cape", "Cloak", "Iron"),
        ("Silver Chest (M)", "Chest", "Silver"),
        ("Silver Cape", "Cloak", "Silver"),
        ("Gold Chest (M)", "Chest", "Gold"),
        ("Gold Cape", "Cloak", "Gold"),
        ("Admantium Chest (M)", "Chest", "Admantium"),
        ("Admantium Cape", "Cloak", "Admantium"),
        ("Legendary Chest (M)", "Chest", "Legendary"),
        ("Legendary Cape", "Cloak", "Legendary"),
    ]
    for c, (nm, piece, mat) in enumerate(armor_pairs):
        tier, base, pool, max_aff = armor_base(piece, mat)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Row 3: helmets + wizard caps
    row = 2
    head_pairs = [
        ("Copper Helmet", "Head", "Copper", False),
        ("Copper Wiz Cap", "Head", "Copper", True),
        ("Iron Helmet", "Head", "Iron", False),
        ("Iron Wizard Cap", "Head", "Iron", True),
        ("Silver Helmet", "Head", "Silver", False),
        ("Silver Wizard Cap", "Head", "Silver", True),
        ("Gold Helmet", "Head", "Gold", False),
        ("Gold Wizard Cap", "Head", "Gold", True),
        ("Admantium Helmet", "Head", "Admantium", False),
        ("Admantium Wizard Cap", "Head", "Admantium", True),
        ("Legendary Helmet", "Head", "Legendary", False),
        ("Legendary Wizard Cap", "Head", "Legendary", True),
    ]
    for c, (nm, piece, mat, wizard) in enumerate(head_pairs):
        if wizard:
            tier, _base, _pool, max_aff = armor_base(piece, mat)
            base = stats(flat={"spellPower": 3 + 3 * RARITY[tier], "armor": 0.5 + 0.4 * RARITY[tier]}, mult={"moveSpeed": 0.01 + 0.01 * RARITY[tier]})
            pool = ["spell_flat_s", "spell_flat_m", "cdr_flat_s", "arcane_res_s", "armor_flat_s"]
            add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for(piece, tier))
        else:
            tier, base, pool, max_aff = armor_base(piece, mat)
            add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for(piece, tier))

    # Row 4: chest (F) + hoods
    row = 3
    chest_hood_pairs = [
        ("Copper Chest (F)", "Chest", "Copper", False),
        ("Copper Hood", "Head", "Copper", True),
        ("Iron Chest (F)", "Chest", "Iron", False),
        ("Iron Hood", "Head", "Iron", True),
        ("Silver Chest (F)", "Chest", "Silver", False),
        ("Silver Hood", "Head", "Silver", True),
        ("Gold Chest (F)", "Chest", "Gold", False),
        ("Gold Hood", "Head", "Gold", True),
        ("Admantium Chest (F)", "Chest", "Admantium", False),
        ("Admantium Hood", "Head", "Admantium", True),
        ("Legendary Chest (F)", "Chest", "Legendary", False),
        ("Legendary Hood", "Head", "Legendary", True),
    ]
    for c, (nm, piece, mat, caster) in enumerate(chest_hood_pairs):
        tier, base, pool, max_aff = armor_base(piece if not caster else "Head", mat)
        if caster:
            base = stats(flat={"spellPower": 2 + 3 * RARITY[tier], "armor": 0.4 + 0.4 * RARITY[tier]}, mult={"moveSpeed": 0.01 + 0.01 * RARITY[tier]})
            pool = ["spell_flat_s", "spell_flat_m", "cdr_flat_s", "arcane_res_s", "frost_res_s"]
            add_item(items, nm, "Head", tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("Head", tier))
        else:
            add_item(items, nm, "Chest", tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("Chest", tier))

    # Row 5: legs + gloves
    row = 4
    legs_gloves = [
        ("Copper Legs", "Legs", "Copper"),
        ("Copper Gloves", "Gloves", "Copper"),
        ("Iron Legs", "Legs", "Iron"),
        ("Iron Gloves", "Gloves", "Iron"),
        ("Silver Legs", "Legs", "Silver"),
        ("Silver Gloves", "Gloves", "Silver"),
        ("Gold Legs", "Legs", "Gold"),
        ("Gold Gloves", "Gloves", "Gold"),
        ("Admantium Legs", "Legs", "Admantium"),
        ("Admantium Gloves", "Gloves", "Admantium"),
        ("Legendary Legs", "Legs", "Legendary"),
        ("Legendary Gloves", "Gloves", "Legendary"),
    ]
    for c, (nm, piece, mat) in enumerate(legs_gloves):
        tier, base, pool, max_aff = armor_base(piece, mat)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Row 6: boots + belts
    row = 5
    boots_belts = [
        ("Copper Boots", "Boots", "Copper"),
        ("Copper Belt", "Belt", "Copper"),
        ("Iron Boots", "Boots", "Iron"),
        ("Iron Belt", "Belt", "Iron"),
        ("Silver Boots", "Boots", "Silver"),
        ("Silver Belt", "Belt", "Silver"),
        ("Gold Boots", "Boots", "Gold"),
        ("Gold Belt", "Belt", "Gold"),
        ("Admantium Boots", "Boots", "Admantium"),
        ("Admantium Belt", "Belt", "Admantium"),
        ("Legendary Boots", "Boots", "Legendary"),
        ("Legendary Belt", "Belt", "Legendary"),
    ]
    for c, (nm, piece, mat) in enumerate(boots_belts):
        tier, base, pool, max_aff = armor_base(piece, mat)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Gems.png (7x2) -> Socketable gems (NOT trinkets)
    gem_rows = [
        ["Small Green Gem", "Small Red Gem", "Small Orange Gem", "Small Purple Gem", "Small Crystal", "Small Jet", "Small Diamond"],
        ["Large Green Gem", "Large Red Gem", "Large Orange Gem", "Large Purple Gem", "Large Crystal", "Large Jet", "Large Diamond"],
    ]
    for r, row_names in enumerate(gem_rows):
        for c, nm in enumerate(row_names):
            if "Small" in nm:
                tier = "Uncommon" if "Diamond" in nm else "Common"
            else:
                tier = "Epic" if "Diamond" in nm else "Rare"
            base = stats()
            if "Green" in nm:
                base = stats(flat={"healthMax": 10 + (10 if "Large" in nm else 0)})
            elif "Red" in nm:
                base = stats(flat={"attackPower": 2 + (2 if "Large" in nm else 0)})
            elif "Orange" in nm:
                base = stats(flat={"spellPower": 3 + (3 if "Large" in nm else 0)})
            elif "Purple" in nm:
                base = stats(flat={"critChance": 0.02 + (0.02 if "Large" in nm else 0)})
            elif "Crystal" in nm:
                base = stats(flat={"armor": 1 + (1 if "Large" in nm else 0), "shieldMax": 10 + (10 if "Large" in nm else 0)})
            elif "Jet" in nm:
                base = stats(flat={"evasion": 2 + (2 if "Large" in nm else 0)})
            elif "Diamond" in nm:
                base = stats(flat={"rarityScore": 0.5 + (0.5 if "Large" in nm else 0)})
            pool = ["rarity_flat_s", "gold_flat_s", "crit_flat_s", "fire_res_s", "frost_res_s"]
            # Keep slot as Trinket in data for simplicity, but mark as socketable so the game blocks equipping.
            add_item(items, nm, "Trinket", tier, "Gems.png", r, c, base, pool, max_affixes_for_rarity(tier, 2 + r),
                     socketable=True, sockets_max=0)

    # Runes.png (6x2) -> Trinkets
    rune_rows = [
        ["Wind Rune", "Damage Rune", "Shield Rune", "Arrow Rune", "Life Rune", "Gold Rune"],
        ["Ghost Rune", "Energy Rune", "Speed Rune", "Star Rune", "Rocket Rune", "Magic Rune"],
    ]
    for r, row_names in enumerate(rune_rows):
        for c, nm in enumerate(row_names):
            tier = "Rare" if r == 0 else "Epic"
            base = stats()
            if nm == "Wind Rune":
                base = stats(mult={"moveSpeed": 0.08})
            elif nm == "Damage Rune":
                base = stats(flat={"attackPower": 6})
            elif nm == "Shield Rune":
                base = stats(flat={"shieldMax": 20})
            elif nm == "Arrow Rune":
                base = stats(flat={"armorPen": 4})
            elif nm == "Life Rune":
                base = stats(flat={"healthMax": 25})
            elif nm == "Gold Rune":
                base = stats(flat={"goldGainMult": 0.08})
            elif nm == "Ghost Rune":
                base = stats(flat={"evasion": 6})
            elif nm == "Energy Rune":
                base = stats(flat={"resourceRegen": 0.12})
            elif nm == "Speed Rune":
                base = stats(mult={"attackSpeed": 0.10})
            elif nm == "Star Rune":
                base = stats(flat={"critChance": 0.06})
            elif nm == "Rocket Rune":
                base = stats(flat={"accuracy": 8})
            elif nm == "Magic Rune":
                base = stats(flat={"spellPower": 12})
            pool = ["rarity_flat_s", "gold_flat_s", "crit_flat_s", "ten_flat_s", "arcane_res_s"]
            add_item(items, nm, "Trinket", tier, "Runes.png", r, c, base, pool, max_affixes_for_rarity(tier, 4),
                     sockets_max=sockets_for("Trinket", tier))

    # Tomes.png (3x2) -> OffHand
    tome_rows = [["Brown Tome", "Red Tome", "Green Magic Tome"], ["Red Magic Tome", "Blue Magic Tome", "Yellow Magic Tome"]]
    tome_tiers = [["Common", "Uncommon", "Rare"], ["Rare", "Epic", "Legendary"]]
    for r, row_names in enumerate(tome_rows):
        for c, nm in enumerate(row_names):
            tier, base, pool, max_aff = offhand_base("tome", tome_tiers[r][c])
            add_item(items, nm, "OffHand", tier, "Tomes.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("OffHand", tier))

    # Shields.png (3x2) -> OffHand
    shield_rows = [["Wooden Round Shield", "Wooden Tower Shield", "Heater Shield"], ["Golden Shield", "Diamond Shield", "Legendary Shield"]]
    shield_tiers = [["Common", "Uncommon", "Rare"], ["Epic", "Legendary", "Legendary"]]
    for r, row_names in enumerate(shield_rows):
        for c, nm in enumerate(row_names):
            tier, base, pool, max_aff = offhand_base("shield", shield_tiers[r][c])
            add_item(items, nm, "OffHand", tier, "Shields.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("OffHand", tier))

    # Quivers.png (3x2) -> OffHand
    quiver_rows = [["Wooden Quiver", "Iron Quiver", "Silver Quiver"], ["Golden Quiver", "Diamond Quiver", "Legendary Quiver"]]
    quiver_tiers = [["Common", "Uncommon", "Rare"], ["Epic", "Legendary", "Legendary"]]
    for r, row_names in enumerate(quiver_rows):
        for c, nm in enumerate(row_names):
            tier, base, pool, max_aff = offhand_base("quiver", quiver_tiers[r][c])
            add_item(items, nm, "OffHand", tier, "Quivers.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("OffHand", tier))

    # Melee_Weapons.png (3x5) -> MainHand
    melee_rows = [
        ["Curved Spear", "Long Spear", "Legendary Spear"],
        ["Wooden Sword", "Iron Sword", "Scimitar"],
        ["Silver Sword", "Bastard Sword", "Beastslayer"],
        ["Zwihander", "Old Blade", "Broadsword"],
        ["Golden Broadsword", "Diamond Broadsword", "Legendary Broadsword"],
    ]
    for r, row_names in enumerate(melee_rows):
        for c, nm in enumerate(row_names):
            if "Legendary" in nm or nm in {"Beastslayer"}:
                tier = "Legendary"
            elif "Diamond" in nm or nm in {"Zwihander"}:
                tier = "Epic"
            elif "Golden" in nm or nm in {"Bastard Sword"}:
                tier = "Epic"
            elif "Silver" in nm or nm in {"Scimitar", "Broadsword"}:
                tier = "Rare"
            elif "Iron" in nm or nm in {"Long Spear"}:
                tier = "Uncommon"
            else:
                tier = "Common"
            tier, base, pool, max_aff = weapon_base("melee", tier)
            add_item(items, nm, "MainHand", tier, "Melee_Weapons.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    # Ranged_Weapons.png (3x5) -> MainHand
    ranged_rows = [
        ["Wooden Crossbow", "Iron Crossbow", "Legndary Crossbow"],
        ["Wooden Bow", "Wooden Longbow", "Iron Longbow"],
        ["Redwood Longbow", "Elven Longbow", "Reinforced Longbow"],
        ["Hunter's Bow", "Diamond String Bow", "Emerald String Bow"],
        ["Golden Longbow", "Diamond Longbow", "Legendary Longbow"],
    ]
    for r, row_names in enumerate(ranged_rows):
        for c, nm in enumerate(row_names):
            if "Legendary" in nm or "Legndary" in nm:
                tier = "Legendary"
            elif "Diamond" in nm or "Emerald" in nm:
                tier = "Epic"
            elif "Golden" in nm:
                tier = "Epic"
            elif nm in {"Elven Longbow", "Reinforced Longbow", "Redwood Longbow"}:
                tier = "Rare"
            elif "Iron" in nm or "Longbow" in nm:
                tier = "Uncommon"
            else:
                tier = "Common"
            tier, base, pool, max_aff = weapon_base("ranged", tier)
            add_item(items, nm, "MainHand", tier, "Ranged_Weapons.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    # Magic_Weapons.png (3x5) -> MainHand
    magic_rows = [
        ["Basic Wand", "Diamond Wand", "Legendary Wand"],
        ["Water Staff", "Aqua Staff", "Staff of the Depths"],
        ["Wooden Staff", "Nature Staff", "Staff of the Oculus"],
        ["Thunder Staff", "Lightning Staff", "Staff of Storms"],
        ["Ember Staff", "Flame Staff", "Staff of the Inferno"],
    ]
    for r, row_names in enumerate(magic_rows):
        for c, nm in enumerate(row_names):
            if "Legendary" in nm or "Inferno" in nm or "Storms" in nm or "Oculus" in nm or "Depths" in nm:
                tier = "Legendary"
            elif "Diamond" in nm or "Lightning" in nm or "Flame" in nm:
                tier = "Epic"
            elif nm in {"Aqua Staff", "Nature Staff", "Thunder Staff", "Ember Staff"}:
                tier = "Rare"
            elif "Wooden" in nm or "Water" in nm or "Basic" in nm:
                tier = "Common"
            else:
                tier = "Uncommon"
            tier, base, pool, max_aff = weapon_base("magic", tier)
            add_item(items, nm, "MainHand", tier, "Magic_Weapons.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    # Arrows.png (5x1) -> Ammo
    arrow_names = ["Wooden Arrow", "Iron Arrow", "Diamond Arrow", "Emerald Arrow", "Legendary Arrow"]
    arrow_tiers = ["Common", "Uncommon", "Epic", "Epic", "Legendary"]
    for c, nm in enumerate(arrow_names):
        tier, base, pool, max_aff = accessory_base("ammo", arrow_tiers[c])
        add_item(items, nm, "Ammo", tier, "Arrows.png", 0, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for("Ammo", tier))

    # Scroll.png (single) -> Trinket
    tier, base, pool, max_aff = accessory_base("trinket", "Rare")
    base = stats(flat={"cooldownReduction": 0.06, "rarityScore": 0.5})
    add_item(items, "Scroll", "Trinket", "Rare", "Scroll.png", 0, 0, base, pool, max_affixes_for_rarity("Rare", max_aff),
             sockets_max=sockets_for("Trinket", "Rare"))

    # Foods.png (2x7) -> Consumable (inventory-only)
    foods = [
        (0, 0, "Carrot", "food_carrot", "Common"),
        (0, 1, "Small Fish", "food_small_fish", "Common"),
        (1, 0, "Orange", "food_orange", "Common"),
        (1, 1, "Blue Fish", "food_blue_fish", "Uncommon"),
        (2, 0, "Strawberry", "food_strawberry", "Uncommon"),
        (2, 1, "Yellow Fish", "food_yellow_fish", "Uncommon"),
        (3, 0, "Watermelon", "food_watermelon", "Uncommon"),
        (3, 1, "Green Fish", "food_green_fish", "Uncommon"),
        (4, 0, "Red Apple", "food_red_apple", "Common"),
        (4, 1, "Clownfish", "food_clownfish", "Rare"),
        (5, 0, "Green Apple", "food_green_apple", "Common"),
        (5, 1, "Cherry", "food_cherry", "Uncommon"),
        (6, 0, "Potato", "food_potato", "Common"),
        (6, 1, "Candy", "food_candy", "Rare"),
    ]
    for r, c, nm, cid, tier in foods:
        add_item(items, nm, "Consumable", tier, "Foods.png", r, c, stats(), [], 0, consumable_id=cid)

    # Potions.png (9x2) -> Consumable (inventory-only)
    sizes = [("Small", 0.25, "Common"), ("Large", 0.50, "Rare"), ("Giant", 0.75, "Epic")]
    caps = [(100, 1.0), (50, 0.5), (25, 0.25)]
    # Row 1: Health potions.
    col = 0
    for size_nm, base_frac, tier in sizes:
        for pct, mul in caps:
            nm = f"{size_nm} Health Potion {pct}%"
            cid = f"potion_hp_{size_nm.lower()}_{pct}"
            add_item(items, nm, "Consumable", tier, "Potions.png", 0, col, stats(), [], 0, consumable_id=cid)
            col += 1
    # Row 2: Rejuvenation (shields) potions.
    col = 0
    for size_nm, base_frac, tier in sizes:
        for pct, mul in caps:
            nm = f"{size_nm} Rejuvenation Potion {pct}%"
            cid = f"potion_sh_{size_nm.lower()}_{pct}"
            add_item(items, nm, "Consumable", tier, "Potions.png", 1, col, stats(), [], 0, consumable_id=cid)
            col += 1

    # Bags.png (8x2) -> Bag
    bags = [
        (0, 0, "Brown Bag", "Epic", 4),
        (1, 0, "Green Bag", "Epic", 8),
        (0, 1, "Blue Bag", "Epic", 16),
        (1, 1, "Red Bag", "Epic", 24),
        (0, 4, "Brown Stringed Bag", "Legendary", 48),
        (1, 4, "Green Stringed Bag", "Legendary", 64),
        (0, 5, "Blue Stringed Bag", "Legendary", 80),
        (1, 5, "Red Stringed Bag", "Legendary", 96),
    ]
    for r, c, nm, tier, slots in bags:
        add_item(items, nm, "Bag", tier, "Bags.png", r, c, stats(), [], 0, bag_slots=slots)

    payload = {"affixes": AFFIXES, "items": items}
    OUT_PATH.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_PATH} ({len(items)} items, {len(AFFIXES)} affixes)")


if __name__ == "__main__":
    main()
