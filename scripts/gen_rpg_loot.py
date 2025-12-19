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

RARITY = {"Common": 0, "Uncommon": 1, "Rare": 2, "Epic": 3, "Legendary": 4, "Unique": 5}


def rarity_from_index(idx: int, total: int) -> str:
    if total <= 0:
        return "Common"
    bucket = min(4, int(idx * 5 / total))
    return ["Common", "Uncommon", "Rare", "Epic", "Legendary"][bucket]


def stats(flat: dict | None = None, mult: dict | None = None) -> dict:
    out: dict = {}
    if flat:
        out["flat"] = flat
    if mult:
        out["mult"] = mult
    return out


def scale_stats(base: dict, flat_mul: float, mult_mul: float = 1.0) -> dict:
    out: dict = {}
    flat = dict(base.get("flat", {}))
    mult = dict(base.get("mult", {}))
    if flat:
        for k, v in flat.items():
            if isinstance(v, (int, float)):
                flat[k] = round(v * flat_mul, 4)
        out["flat"] = flat
    if mult:
        for k, v in mult.items():
            if isinstance(v, (int, float)):
                mult[k] = round(v * mult_mul, 4)
        out["mult"] = mult
    return out


def flat_resists(**kwargs: float) -> dict:
    return {"resists": kwargs}


AFFIXES = [
    # Core flat stats (tiered)
    {"id": "atk_flat_s", "name": "+3 Attack Power", "stats": stats(flat={"attackPower": 3})},
    {"id": "atk_flat_m", "name": "+6 Attack Power", "stats": stats(flat={"attackPower": 6})},
    {"id": "atk_flat_l", "name": "+9 Attack Power", "stats": stats(flat={"attackPower": 9})},
    {"id": "spell_flat_s", "name": "+4 Spell Power", "stats": stats(flat={"spellPower": 4})},
    {"id": "spell_flat_m", "name": "+8 Spell Power", "stats": stats(flat={"spellPower": 8})},
    {"id": "spell_flat_l", "name": "+12 Spell Power", "stats": stats(flat={"spellPower": 12})},
    {"id": "armor_flat_s", "name": "+1 Armor", "stats": stats(flat={"armor": 1})},
    {"id": "armor_flat_m", "name": "+2 Armor", "stats": stats(flat={"armor": 2})},
    {"id": "armor_flat_l", "name": "+4 Armor", "stats": stats(flat={"armor": 4})},
    {"id": "hp_flat_s", "name": "+12 Max Health", "stats": stats(flat={"healthMax": 12})},
    {"id": "hp_flat_m", "name": "+25 Max Health", "stats": stats(flat={"healthMax": 25})},
    {"id": "hp_flat_l", "name": "+50 Max Health", "stats": stats(flat={"healthMax": 50})},
    {"id": "shield_flat_s", "name": "+12 Max Shields", "stats": stats(flat={"shieldMax": 12})},
    {"id": "shield_flat_m", "name": "+25 Max Shields", "stats": stats(flat={"shieldMax": 25})},
    {"id": "shield_flat_l", "name": "+50 Max Shields", "stats": stats(flat={"shieldMax": 50})},
    {"id": "acc_flat_s", "name": "+4 Accuracy", "stats": stats(flat={"accuracy": 4})},
    {"id": "acc_flat_m", "name": "+8 Accuracy", "stats": stats(flat={"accuracy": 8})},
    {"id": "acc_flat_l", "name": "+12 Accuracy", "stats": stats(flat={"accuracy": 12})},
    {"id": "ten_flat_s", "name": "+4 Tenacity", "stats": stats(flat={"tenacity": 4})},
    {"id": "ten_flat_m", "name": "+8 Tenacity", "stats": stats(flat={"tenacity": 8})},
    {"id": "ten_flat_l", "name": "+12 Tenacity", "stats": stats(flat={"tenacity": 12})},
    {"id": "eva_flat_s", "name": "+3 Evasion", "stats": stats(flat={"evasion": 3})},
    {"id": "eva_flat_m", "name": "+6 Evasion", "stats": stats(flat={"evasion": 6})},
    {"id": "eva_flat_l", "name": "+9 Evasion", "stats": stats(flat={"evasion": 9})},
    {"id": "arp_flat_s", "name": "+3 Armor Pen", "stats": stats(flat={"armorPen": 3})},
    {"id": "arp_flat_m", "name": "+6 Armor Pen", "stats": stats(flat={"armorPen": 6})},
    {"id": "arp_flat_l", "name": "+9 Armor Pen", "stats": stats(flat={"armorPen": 9})},
    # Multipliers / percent-style stats
    {"id": "as_mult_s", "name": "+6% Attack Speed", "stats": stats(mult={"attackSpeed": 0.06})},
    {"id": "as_mult_m", "name": "+10% Attack Speed", "stats": stats(mult={"attackSpeed": 0.10})},
    {"id": "as_mult_l", "name": "+14% Attack Speed", "stats": stats(mult={"attackSpeed": 0.14})},
    {"id": "ms_mult_s", "name": "+5% Move Speed", "stats": stats(mult={"moveSpeed": 0.05})},
    {"id": "ms_mult_m", "name": "+8% Move Speed", "stats": stats(mult={"moveSpeed": 0.08})},
    {"id": "ms_mult_l", "name": "+12% Move Speed", "stats": stats(mult={"moveSpeed": 0.12})},
    {"id": "regen_mult_s", "name": "+8% Resource Regen", "stats": stats(mult={"resourceRegen": 0.08})},
    {"id": "regen_mult_m", "name": "+15% Resource Regen", "stats": stats(mult={"resourceRegen": 0.15})},
    {"id": "regen_mult_l", "name": "+25% Resource Regen", "stats": stats(mult={"resourceRegen": 0.25})},
    {"id": "crit_flat_s", "name": "+3% Crit Chance", "stats": stats(flat={"critChance": 0.03})},
    {"id": "crit_flat_m", "name": "+6% Crit Chance", "stats": stats(flat={"critChance": 0.06})},
    {"id": "crit_flat_l", "name": "+9% Crit Chance", "stats": stats(flat={"critChance": 0.09})},
    {"id": "cdr_flat_s", "name": "+3% Cooldown Reduction", "stats": stats(flat={"cooldownReduction": 0.03})},
    {"id": "cdr_flat_m", "name": "+6% Cooldown Reduction", "stats": stats(flat={"cooldownReduction": 0.06})},
    {"id": "cdr_flat_l", "name": "+9% Cooldown Reduction", "stats": stats(flat={"cooldownReduction": 0.09})},
    {"id": "gold_flat_s", "name": "+5% Gold Gain", "stats": stats(flat={"goldGainMult": 0.05})},
    {"id": "gold_flat_m", "name": "+10% Gold Gain", "stats": stats(flat={"goldGainMult": 0.10})},
    {"id": "gold_flat_l", "name": "+15% Gold Gain", "stats": stats(flat={"goldGainMult": 0.15})},
    {"id": "rarity_flat_s", "name": "+0.6 Rarity Score", "stats": stats(flat={"rarityScore": 0.6})},
    {"id": "rarity_flat_m", "name": "+1.2 Rarity Score", "stats": stats(flat={"rarityScore": 1.2})},
    {"id": "rarity_flat_l", "name": "+2.0 Rarity Score", "stats": stats(flat={"rarityScore": 2.0})},
    # Resists
    {"id": "fire_res_s", "name": "+8% Fire Resist", "stats": stats(flat=flat_resists(Fire=0.08))},
    {"id": "fire_res_m", "name": "+12% Fire Resist", "stats": stats(flat=flat_resists(Fire=0.12))},
    {"id": "frost_res_s", "name": "+8% Frost Resist", "stats": stats(flat=flat_resists(Frost=0.08))},
    {"id": "frost_res_m", "name": "+12% Frost Resist", "stats": stats(flat=flat_resists(Frost=0.12))},
    {"id": "shock_res_s", "name": "+8% Shock Resist", "stats": stats(flat=flat_resists(Shock=0.08))},
    {"id": "shock_res_m", "name": "+12% Shock Resist", "stats": stats(flat=flat_resists(Shock=0.12))},
    {"id": "poison_res_s", "name": "+8% Poison Resist", "stats": stats(flat=flat_resists(Poison=0.08))},
    {"id": "poison_res_m", "name": "+12% Poison Resist", "stats": stats(flat=flat_resists(Poison=0.12))},
    {"id": "arcane_res_s", "name": "+8% Arcane Resist", "stats": stats(flat=flat_resists(Arcane=0.08))},
    {"id": "arcane_res_m", "name": "+12% Arcane Resist", "stats": stats(flat=flat_resists(Arcane=0.12))},
    # All resist
    {"id": "all_res_s", "name": "+4% All Resist", "stats": stats(flat=flat_resists(Fire=0.04, Frost=0.04, Shock=0.04, Poison=0.04, Arcane=0.04))},
    {"id": "all_res_m", "name": "+6% All Resist", "stats": stats(flat=flat_resists(Fire=0.06, Frost=0.06, Shock=0.06, Poison=0.06, Arcane=0.06))},
    # Hybrid affixes
    {"id": "duelist_s", "name": "+3 Attack Power, +3 Evasion", "stats": stats(flat={"attackPower": 3, "evasion": 3})},
    {"id": "marksman_s", "name": "+4 Accuracy, +3 Armor Pen", "stats": stats(flat={"accuracy": 4, "armorPen": 3})},
    {"id": "assassin_s", "name": "+6% Attack Speed, +3% Crit Chance", "stats": stats(flat={"critChance": 0.03}, mult={"attackSpeed": 0.06})},
    {"id": "executioner_s", "name": "+6 Attack Power, +3 Armor Pen", "stats": stats(flat={"attackPower": 6, "armorPen": 3})},
    {"id": "arcanist_s", "name": "+8 Spell Power, +3% Cooldown Reduction", "stats": stats(flat={"spellPower": 8, "cooldownReduction": 0.03})},
    {"id": "bulwark_s", "name": "+25 Max Health, +2 Armor", "stats": stats(flat={"healthMax": 25, "armor": 2})},
    {"id": "warded_s", "name": "+25 Max Shields, +8% Arcane Resist", "stats": stats(flat={**flat_resists(Arcane=0.08), "shieldMax": 25})},
    {"id": "stalwart_s", "name": "+12 Max Health, +4 Tenacity", "stats": stats(flat={"healthMax": 12, "tenacity": 4})},
    {"id": "elusive_s", "name": "+5% Move Speed, +3 Evasion", "stats": stats(flat={"evasion": 3}, mult={"moveSpeed": 0.05})},
    {"id": "tactician_s", "name": "+3% Cooldown Reduction, +8% Resource Regen", "stats": stats(flat={"cooldownReduction": 0.03}, mult={"resourceRegen": 0.08})},
    {"id": "prospector_s", "name": "+5% Gold Gain, +0.6 Rarity Score", "stats": stats(flat={"goldGainMult": 0.05, "rarityScore": 0.6})},
    {"id": "precision_s", "name": "+4 Accuracy, +3% Crit Chance", "stats": stats(flat={"accuracy": 4, "critChance": 0.03})},
]


ARMOR_TIER = {
    "Leather": ("Common", 1.0),
    "Iron": ("Uncommon", 2.0),
    "Silver": ("Rare", 3.0),
    "Gold": ("Epic", 4.0),
    "Admantium": ("Epic", 5.0),
    "Sunsteel": ("Legendary", 6.0),
}


def armor_base(piece: str, tier_name: str, name: str) -> tuple[str, dict, list[str], int]:
    rarity, _tier = ARMOR_TIER[tier_name]
    pool_def = ["armor_flat_s", "armor_flat_m", "armor_flat_l", "hp_flat_s", "hp_flat_m", "hp_flat_l",
                "ten_flat_s", "ten_flat_m", "ten_flat_l", "fire_res_s", "fire_res_m", "frost_res_s", "frost_res_m",
                "all_res_s", "all_res_m", "bulwark_s", "stalwart_s"]
    pool_magic = ["spell_flat_s", "spell_flat_m", "spell_flat_l", "cdr_flat_s", "cdr_flat_m", "cdr_flat_l",
                  "arcane_res_s", "arcane_res_m", "regen_mult_s", "regen_mult_m", "regen_mult_l", "all_res_s", "all_res_m",
                  "warded_s", "arcanist_s", "tactician_s"]
    pool_agile = ["eva_flat_s", "eva_flat_m", "eva_flat_l", "ms_mult_s", "ms_mult_m", "ms_mult_l",
                  "as_mult_s", "as_mult_m", "as_mult_l", "poison_res_s", "poison_res_m", "all_res_s", "all_res_m",
                  "elusive_s", "duelist_s", "precision_s"]

    nm = name.lower()
    if piece == "Head":
        if "hood" in nm or "wizard" in nm:
            base = stats(flat={"evasion": 3, **flat_resists(Poison=0.08)})
            return rarity, base, pool_agile, 2
        base = stats(flat={"armor": 2, "tenacity": 4})
        return rarity, base, pool_def, 2
    if piece == "Chest":
        if "leather" in nm:
            base = stats(flat={"armor": 2, "evasion": 3})
            return rarity, base, pool_agile, 2
        if "(f)" in nm or "robe" in nm:
            base = stats(flat={"healthMax": 12, "spellPower": 4, **flat_resists(Arcane=0.08)})
            return rarity, base, pool_magic, 2
        base = stats(flat={"armor": 3, "healthMax": 25})
        return rarity, base, pool_def, 2
    if piece == "Boots":
        if "leather" in nm or "light" in nm:
            base = stats(flat={"evasion": 3}, mult={"moveSpeed": 0.05})
            return rarity, base, pool_agile, 2
        base = stats(flat={"armor": 1}, mult={"moveSpeed": 0.05})
        return rarity, base, pool_def, 2
    if piece == "Gloves":
        if "leather" in nm:
            base = stats(mult={"attackSpeed": 0.06})
            return rarity, base, pool_agile, 2
        if "admantium" in nm or "sunsteel" in nm or "mystic" in nm:
            base = stats(flat={"spellPower": 4}, mult={"resourceRegen": 0.08})
            return rarity, base, pool_magic, 2
        base = stats(flat={"attackPower": 3, "armor": 1})
        return rarity, base, pool_def, 2
    if piece == "Belt":
        if "leather" in nm:
            base = stats(mult={"resourceRegen": 0.08})
            return rarity, base, pool_magic, 2
        if "admantium" in nm or "sunsteel" in nm or "arcane" in nm:
            base = stats(flat={"shieldMax": 25})
            return rarity, base, pool_magic, 2
        base = stats(flat={"healthMax": 25})
        return rarity, base, pool_def, 2
    if piece == "Cloak":
        # Preserve existing cloak identity: agile/resist-focused.
        base = stats(flat={"armor": 1, "evasion": 3}, mult={"moveSpeed": 0.04})
        return rarity, base, pool_agile, 2
    if piece == "Legs":
        base = stats(flat={"armor": 2, "healthMax": 18})
        return rarity, base, pool_def, 2
    base = stats(flat={"armor": 1})
    return rarity, base, pool_def, 2


def weapon_base(kind: str, tier: str, name: str) -> tuple[str, dict, list[str], int]:
    nm = name.lower()
    pool_melee = ["atk_flat_s", "atk_flat_m", "atk_flat_l", "as_mult_s", "as_mult_m", "as_mult_l",
                  "crit_flat_s", "crit_flat_m", "crit_flat_l", "arp_flat_s", "arp_flat_m", "arp_flat_l",
                  "acc_flat_s", "acc_flat_m", "acc_flat_l", "executioner_s", "duelist_s", "precision_s"]
    pool_ranged = ["atk_flat_s", "atk_flat_m", "atk_flat_l", "as_mult_s", "as_mult_m", "as_mult_l",
                   "ms_mult_s", "ms_mult_m", "ms_mult_l", "crit_flat_s", "crit_flat_m", "crit_flat_l",
                   "arp_flat_s", "arp_flat_m", "arp_flat_l", "acc_flat_s", "acc_flat_m", "acc_flat_l",
                   "marksman_s", "precision_s"]
    pool_magic = ["spell_flat_s", "spell_flat_m", "spell_flat_l", "cdr_flat_s", "cdr_flat_m", "cdr_flat_l",
                  "regen_mult_s", "regen_mult_m", "regen_mult_l", "arcanist_s", "tactician_s",
                  "arcane_res_s", "arcane_res_m", "all_res_s"]

    if kind == "magic":
        if "wand" in nm:
            base = stats(flat={"spellPower": 12, "cooldownReduction": 0.03})
        else:
            base = stats(flat={"spellPower": 14}, mult={"resourceRegen": 0.08})
        return tier, base, pool_magic, max_affixes_for_rarity(tier, RARITY[tier] + 1)
    if kind == "ranged":
        base = stats(flat={"attackPower": 10, "accuracy": 4})
        return tier, base, pool_ranged, max_affixes_for_rarity(tier, RARITY[tier] + 1)
    # melee
    if "shortsword" in nm or "rapier" in nm or "dagger" in nm:
        base = stats(flat={"attackPower": 9}, mult={"attackSpeed": 0.06})
    else:
        base = stats(flat={"attackPower": 12})
    return tier, base, pool_melee, max_affixes_for_rarity(tier, RARITY[tier] + 1)


def offhand_base(kind: str, tier: str) -> tuple[str, dict, list[str], int]:
    pool_def = ["armor_flat_s", "armor_flat_m", "armor_flat_l", "shield_flat_s", "shield_flat_m", "shield_flat_l",
                "ten_flat_s", "ten_flat_m", "ten_flat_l", "all_res_s", "all_res_m", "bulwark_s", "stalwart_s"]
    pool_magic = ["cdr_flat_s", "cdr_flat_m", "cdr_flat_l", "regen_mult_s", "regen_mult_m", "regen_mult_l",
                  "arcane_res_s", "arcane_res_m", "all_res_s", "all_res_m", "tactician_s"]
    pool_ranged = ["crit_flat_s", "crit_flat_m", "crit_flat_l", "acc_flat_s", "acc_flat_m", "acc_flat_l",
                   "marksman_s", "precision_s", "assassin_s"]
    if kind == "shield":
        return tier, stats(flat={"armor": 2, "shieldMax": 25}), pool_def, max_affixes_for_rarity(tier, 2)
    if kind == "quiver":
        return tier, stats(flat={"accuracy": 4, "critChance": 0.03}), pool_ranged, max_affixes_for_rarity(tier, 2)
    # talisman (tomes)
    return tier, stats(flat={"cooldownReduction": 0.03, **flat_resists(Arcane=0.08)}), pool_magic, max_affixes_for_rarity(tier, 2)


def accessory_base(kind: str, tier: str, name: str) -> tuple[str, dict, list[str], int]:
    if kind == "ammo":
        return tier, stats(flat={"attackPower": 2, "armorPen": 2}), ["atk_flat_s", "crit_flat_s", "arp_flat_s", "marksman_s"], max_affixes_for_rarity(tier, 2)
    if kind == "ring":
        nm = name.lower()
        if "emerald" in nm or "garnet" in nm:
            base = stats(flat={"attackPower": 3})
        elif "sapphire" in nm or "amethyst" in nm:
            base = stats(flat={"spellPower": 4})
        else:
            base = stats(flat={"critChance": 0.03})
        pool = ["crit_flat_s", "crit_flat_m", "crit_flat_l", "atk_flat_s", "atk_flat_m", "atk_flat_l",
                "spell_flat_s", "spell_flat_m", "spell_flat_l", "rarity_flat_s", "rarity_flat_m", "rarity_flat_l",
                "precision_s"]
        return tier, base, pool, max_affixes_for_rarity(tier, 2)
    if kind == "amulet":
        nm = name.lower()
        if "onyx" in nm or "sapphire" in nm or "amethyst" in nm:
            base = stats(flat={"cooldownReduction": 0.03})
        else:
            base = stats(mult={"resourceRegen": 0.08})
        pool = ["cdr_flat_s", "cdr_flat_m", "cdr_flat_l", "regen_mult_s", "regen_mult_m", "regen_mult_l",
                "arcane_res_s", "arcane_res_m", "all_res_s", "tactician_s"]
        return tier, base, pool, max_affixes_for_rarity(tier, 2)
    # trinket/charm baseline (loot/econ)
    base = stats(flat={"goldGainMult": 0.05})
    pool = ["gold_flat_s", "gold_flat_m", "gold_flat_l", "rarity_flat_s", "rarity_flat_m", "rarity_flat_l", "prospector_s"]
    return tier, base, pool, max_affixes_for_rarity(tier, 2)


def max_affixes_for_rarity(tier: str, suggested: int) -> int:
    return max(1, min(suggested, 1 + RARITY[tier]))


def max_affixes_for_unique(suggested: int) -> int:
    return max_affixes_for_rarity("Legendary", suggested) + 1


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
    ring_names = ["Emerald Ring", "Sapphire Ring", "Garnet Ring", "Amethyst Ring", "Onyx Ring", "Citrine Ring"]
    ring_rarity = {"Onyx Ring": "Common", "Emerald Ring": "Uncommon", "Sapphire Ring": "Rare", "Citrine Ring": "Rare", "Garnet Ring": "Epic", "Amethyst Ring": "Legendary"}
    for c, nm in enumerate(ring_names):
        tier, base, pool, max_aff = accessory_base("ring", ring_rarity[nm], nm)
        add_item(items, nm, "Ring1", tier, "Gear.png", 0, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for("Ring1", tier))

    amulet_names = ["Emerald Amulet", "Onyx Amulet", "Old Amulet", "Sapphire Amulet", "Sunsteel Amulet", "Amethyst Amulet"]
    amulet_rarity = {"Emerald Amulet": "Common", "Onyx Amulet": "Uncommon", "Old Amulet": "Common", "Sapphire Amulet": "Epic",
                     "Sunsteel Amulet": "Rare", "Amethyst Amulet": "Legendary"}
    for c, nm in enumerate(amulet_names, start=6):
        tier, base, pool, max_aff = accessory_base("amulet", amulet_rarity[nm], nm)
        add_item(items, nm, "Amulet", tier, "Gear.png", 0, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for("Amulet", tier))

    charm_pool = ["gold_flat_s", "gold_flat_m", "rarity_flat_s", "rarity_flat_m", "prospector_s"]
    add_item(items, "Prosperity Charm", "Trinket", "Uncommon", "Gear.png", 0, 5, stats(flat={"goldGainMult": 0.05}), charm_pool, 1)
    add_item(items, "Relic Charm", "Trinket", "Rare", "Gear.png", 0, 11, stats(flat={"rarityScore": 0.6}), charm_pool, 1)

    # Row 2: chest (M) + capes
    row = 1
    armor_pairs = [
        ("Leather Chest (M)", "Chest", "Leather"),
        ("Leather Cape", "Cloak", "Leather"),
        ("Iron Chest (M)", "Chest", "Iron"),
        ("Black Cape", "Cloak", "Iron"),
        ("Silver Chest (M)", "Chest", "Silver"),
        ("Grey Cape", "Cloak", "Silver"),
        ("Gold Chest (M)", "Chest", "Gold"),
        ("Golden Cape", "Cloak", "Gold"),
        ("Admantium Chest (M)", "Chest", "Admantium"),
        ("Silk Cape", "Cloak", "Admantium"),
        ("Sunsteel Chest (M)", "Chest", "Sunsteel"),
        ("Sunsilk Cape", "Cloak", "Sunsteel"),
    ]
    for c, (nm, piece, mat) in enumerate(armor_pairs):
        tier, base, pool, max_aff = armor_base(piece, mat, nm)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Row 3: helmets + wizard caps
    row = 2
    head_pairs = [
        ("Leather Helmet", "Head", "Leather", False),
        ("Leather Hood", "Head", "Leather", True),
        ("Iron Helmet", "Head", "Iron", False),
        ("Iron Hood", "Head", "Iron", True),
        ("Silver Helmet", "Head", "Silver", False),
        ("Silver Hood", "Head", "Silver", True),
        ("Gold Helmet", "Head", "Gold", False),
        ("Gold Hood", "Head", "Gold", True),
        ("Admantium Helmet", "Head", "Admantium", False),
        ("Admantium Hood", "Head", "Admantium", True),
        ("Sunsteel Helmet", "Head", "Sunsteel", False),
        ("Sunsteel Hood", "Head", "Sunsteel", True),
    ]
    for c, (nm, piece, mat, wizard) in enumerate(head_pairs):
        tier, base, pool, max_aff = armor_base(piece, mat, nm)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Row 4: chest (F) + hoods
    row = 3
    chest_hood_pairs = [
        ("Leather Chest (F)", "Chest", "Leather", False),
        ("Leather Hood", "Head", "Leather", True),
        ("Iron Chest (F)", "Chest", "Iron", False),
        ("Iron Hood", "Head", "Iron", True),
        ("Silver Chest (F)", "Chest", "Silver", False),
        ("Silver Hood", "Head", "Silver", True),
        ("Gold Chest (F)", "Chest", "Gold", False),
        ("Gold Hood", "Head", "Gold", True),
        ("Admantium Chest (F)", "Chest", "Admantium", False),
        ("Admantium Hood", "Head", "Admantium", True),
        ("Sunsteel Chest (F)", "Chest", "Sunsteel", False),
        ("Sunsteel Hood", "Head", "Sunsteel", True),
    ]
    for c, (nm, piece, mat, caster) in enumerate(chest_hood_pairs):
        tier, base, pool, max_aff = armor_base(piece if not caster else "Head", mat, nm)
        if caster:
            add_item(items, nm, "Head", tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("Head", tier))
        else:
            add_item(items, nm, "Chest", tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("Chest", tier))

    # Row 5: legs + gloves
    row = 4
    legs_gloves = [
        ("Leather Legs", "Legs", "Leather"),
        ("Leather Gloves", "Gloves", "Leather"),
        ("Iron Legs", "Legs", "Iron"),
        ("Iron Gloves", "Gloves", "Iron"),
        ("Silver Legs", "Legs", "Silver"),
        ("Silver Gloves", "Gloves", "Silver"),
        ("Gold Legs", "Legs", "Gold"),
        ("Gold Gloves", "Gloves", "Gold"),
        ("Admantium Legs", "Legs", "Admantium"),
        ("Admantium Gloves", "Gloves", "Admantium"),
        ("Sunsteel Legs", "Legs", "Sunsteel"),
        ("Sunsteel Gloves", "Gloves", "Sunsteel"),
    ]
    for c, (nm, piece, mat) in enumerate(legs_gloves):
        tier, base, pool, max_aff = armor_base(piece, mat, nm)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Row 6: boots + belts
    row = 5
    boots_belts = [
        ("Leather Boots", "Boots", "Leather"),
        ("Leather Belt", "Belt", "Leather"),
        ("Iron Boots", "Boots", "Iron"),
        ("Iron Belt", "Belt", "Iron"),
        ("Silver Boots", "Boots", "Silver"),
        ("Silver Belt", "Belt", "Silver"),
        ("Gold Boots", "Boots", "Gold"),
        ("Gold Belt", "Belt", "Gold"),
        ("Admantium Boots", "Boots", "Admantium"),
        ("Admantium Belt", "Belt", "Admantium"),
        ("Sunsteel Boots", "Boots", "Sunsteel"),
        ("Sunsteel Belt", "Belt", "Sunsteel"),
    ]
    for c, (nm, piece, mat) in enumerate(boots_belts):
        tier, base, pool, max_aff = armor_base(piece, mat, nm)
        add_item(items, nm, piece, tier, "Gear.png", row, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for(piece, tier))

    # Gems.png (7x2) -> Socketable gems (NOT trinkets)
    gem_rows = [
        ["Verdant Chip", "Crimson Chip", "Amber Chip", "Violet Chip", "Prism Chip", "Onyx Chip", "Radiant Chip"],
        ["Verdant Core", "Crimson Core", "Amber Core", "Violet Core", "Prism Core", "Onyx Core", "Radiant Core"],
    ]
    for r, row_names in enumerate(gem_rows):
        for c, nm in enumerate(row_names):
            if "Chip" in nm:
                tier = "Uncommon" if "Radiant" in nm else "Common"
            else:
                tier = "Epic" if "Radiant" in nm else "Rare"
            base = stats()
            if "Verdant" in nm:
                base = stats(flat={"healthMax": 10 + (10 if "Core" in nm else 0)})
            elif "Crimson" in nm:
                base = stats(flat={"attackPower": 2 + (2 if "Core" in nm else 0)})
            elif "Amber" in nm:
                base = stats(flat={"spellPower": 3 + (3 if "Core" in nm else 0)})
            elif "Violet" in nm:
                base = stats(flat={"critChance": 0.02 + (0.02 if "Core" in nm else 0)})
            elif "Prism" in nm:
                base = stats(flat={"armor": 1 + (1 if "Core" in nm else 0), "shieldMax": 10 + (10 if "Core" in nm else 0)})
            elif "Onyx" in nm:
                base = stats(flat={"evasion": 2 + (2 if "Core" in nm else 0)})
            elif "Radiant" in nm:
                base = stats(flat={"rarityScore": 0.5 + (0.5 if "Core" in nm else 0)})
            # Keep slot as Trinket in data for simplicity, but mark as socketable so the game blocks equipping.
            add_item(items, nm, "Trinket", tier, "Gems.png", r, c, base, [], 0,
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
            pool = ["rarity_flat_s", "rarity_flat_m", "gold_flat_s", "gold_flat_m", "crit_flat_s", "crit_flat_m",
                    "ten_flat_s", "ten_flat_m", "arcane_res_s", "arcane_res_m"]
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
    quiver_rows = [["Wooden Quiver", "Iron Quiver", "Silver Quiver"], ["Golden Quiver", "Diamond Quiver", "Sunsteel Quiver"]]
    quiver_tiers = [["Common", "Uncommon", "Rare"], ["Epic", "Legendary", "Legendary"]]
    for r, row_names in enumerate(quiver_rows):
        for c, nm in enumerate(row_names):
            tier, base, pool, max_aff = offhand_base("quiver", quiver_tiers[r][c])
            add_item(items, nm, "OffHand", tier, "Quivers.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("OffHand", tier))

    # Melee_Weapons.png (3x5) -> MainHand
    melee_rows = [
        ["Curved Spear", "Long Spear", "Sunsteel Spear"],
        ["Wooden Sword", "Iron Sword", "Scimitar"],
        ["Silver Sword", "Bastard Sword", "Beastslayer"],
        ["Zwihander", "Old Blade", "Broadsword"],
        ["Golden Broadsword", "Diamond Broadsword", "Sunsteel Broadsword"],
    ]
    for r, row_names in enumerate(melee_rows):
        for c, nm in enumerate(row_names):
            if "Legendary" in nm or "Sunsteel" in nm or nm in {"Beastslayer"}:
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
            tier, base, pool, max_aff = weapon_base("melee", tier, nm)
            add_item(items, nm, "MainHand", tier, "Melee_Weapons.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    # Ranged_Weapons.png (3x5) -> MainHand
    ranged_rows = [
        ["Wooden Crossbow", "Iron Crossbow", "Sunsteel Crossbow"],
        ["Wooden Bow", "Wooden Longbow", "Iron Longbow"],
        ["Redwood Longbow", "Elven Longbow", "Reinforced Longbow"],
        ["Hunter's Bow", "Diamond String Bow", "Emerald String Bow"],
        ["Golden Longbow", "Diamond Longbow", "Sunsteel Longbow"],
    ]
    for r, row_names in enumerate(ranged_rows):
        for c, nm in enumerate(row_names):
            if "Legendary" in nm or "Legndary" in nm or "Sunsteel" in nm:
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
            tier, base, pool, max_aff = weapon_base("ranged", tier, nm)
            add_item(items, nm, "MainHand", tier, "Ranged_Weapons.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    # Magic_Weapons.png (3x5) -> MainHand
    magic_rows = [
        ["Basic Wand", "Diamond Wand", "Sunsteel Wand"],
        ["Water Staff", "Aqua Staff", "Staff of the Depths"],
        ["Wooden Staff", "Nature Staff", "Staff of the Oculus"],
        ["Thunder Staff", "Lightning Staff", "Staff of Storms"],
        ["Ember Staff", "Flame Staff", "Staff of the Inferno"],
    ]
    for r, row_names in enumerate(magic_rows):
        for c, nm in enumerate(row_names):
            if "Legendary" in nm or "Sunsteel" in nm or "Inferno" in nm or "Storms" in nm or "Oculus" in nm or "Depths" in nm:
                tier = "Legendary"
            elif "Diamond" in nm or "Lightning" in nm or "Flame" in nm:
                tier = "Epic"
            elif nm in {"Aqua Staff", "Nature Staff", "Thunder Staff", "Ember Staff"}:
                tier = "Rare"
            elif "Wooden" in nm or "Water" in nm or "Basic" in nm:
                tier = "Common"
            else:
                tier = "Uncommon"
            tier, base, pool, max_aff = weapon_base("magic", tier, nm)
            add_item(items, nm, "MainHand", tier, "Magic_Weapons.png", r, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    # New weapon sheets (melee) -> MainHand
    def add_weapon_sheet(names: list[str], cols: int, sheet: str) -> None:
        total = len(names)
        for idx, nm in enumerate(names):
            tier = rarity_from_index(idx, total)
            tier, base, pool, max_aff = weapon_base("melee", tier, nm)
            row = idx // cols
            col = idx % cols
            add_item(items, nm, "MainHand", tier, sheet, row, col, base, pool, max_affixes_for_rarity(tier, max_aff),
                     sockets_max=sockets_for("MainHand", tier))

    add_weapon_sheet([
        "Emberwood Hatchet", "Ironhook Handaxe", "Titancleave Greataxe",
        "Stonebiter Hatchet", "Skyspike Hookaxe", "Frostbeard Battleaxe",
        "Mooncrescent Reaver", "Ash-Raider Waraxe", "Stormrend Greatblade",
        "Quarrymaul Chopper", "Deepcut Bearded Axe", "Frostjaw Waraxe",
        "Pikehead Splitter", "Blackforge Broadaxe", "Oathbreaker Cleaver",
        "Driftsteel Hatchet", "Ravager's Beak", "Silveredge Handaxe",
        "Gravelgnash Axe", "Twinwing Double-Bit", "Nightreap Crescent Axe",
        "Mountainbreaker Greataxe", "Ironbloom Cleaver", "Hookfang Waraxe",
        "Coldsteel Executioner", "Glacierfall Greataxe", "Gilded Twin-Fang",
        "Warlord's Chopper", "Thunderhead Greataxe", "Leviathan Cleaver",
    ], 3, "Axes.png")

    add_weapon_sheet([
        "Hearthsteel Cleaver", "Stoneblock Chopper", "Emberhook Butcher", "Duskcurve Cleaver",
        "Searmark Cleaver", "Porksplitter", "Ash-Hew Chopper", "Smokewedge Cleaver",
        "Bone-Shear", "Tallowcutter", "Rib-Ripper", "Marrowblade",
        "Chopstorm Cleaver", "Slab-Slayer", "Hookjaw Cleaver", "Wharfside Chopper",
        "Gristlegrinder", "Broadbite Cleaver", "Cartilage Cutter", "Twin-Weight Chopper",
        "Dockhand's Cleaver", "Ironpantry Chopper", "Smelter's Butcher", "Guttersteel Cleaver",
        "Cinderchop Cleaver", "Coldblock Cleaver", "Brinehook Butcher", "Saltknife Cleaver",
        "Spinesplitter", "Heap-Hewer", "Meathook Reaper", "Marketday Cleaver",
        "Ashen Carver", "Butcher's Oath", "Furnacefang Cleaver", "Hammeredge Chopper",
        "Lowroad Cleaver", "Black Apron Blade", "Graveyard Butcher", "King's Kitchen Guillotine",
    ], 4, "Cleavers.png")

    add_weapon_sheet([
        "Tavern-Brawl Baton", "Cobbleknob Club", "Gutterpick Mace", "Knife-Edge Cudgel",
        "Old Faithful Cudgel", "Gravelspike Crusher", "Anvilcap Maul", "Rustfang Baton",
        "Oakbone Bludgeon", "Squarehead Smiter", "Rivet-Maul Mace", "Gleamshiv Club",
        "Brawler's Branch", "Cornerstone Hammer", "Morningstar Knocker", "Dockside Shivstick",
        "Ironknob Cudgel", "Pebblemaul", "Caltrop Crown Mace", "Shortbite Club",
        "Stonejaw Baton", "Knuckleknock Mace", "Thornwheel Morningstar", "Brickbat Club",
        "Golemfist Maul", "Roundhead Smasher", "Warder's Flange", "Wickedspike Pick-Mace",
        "Hexhead Bruiser", "Bulwark Knocker", "Stitchspike Star", "Backalley Bladebat",
        "Grudgewood Club", "Cinderblock Smiter", "Crownbreaker Mace", "Ironfang Baton",
        "Big Stick Energy", "Sunburst Smasher", "Thistlemaul Morningstar", "Ogre's Tooth Club",
    ], 4, "Clubs.png")

    add_weapon_sheet([
        "Rivet-Tap Hammer", "Hookbeak Warhammer", "Quarryblock Maul", "Nailbiter Mallet",
        "Anvilfang Hammer", "Sunforged Sledge", "Cobblecracker", "Splitjaw Maul",
        "Ironkeep Hammer", "Backhand Basher", "Gilded Pounder", "Darkforge Maul",
        "Stonethud Hammer", "Knuckledrum Maul", "Cragspike Warhammer", "Bricklayer's Sledge",
        "Oresplitter Maul", "Grave-Anvil Hammer", "Hearthmaul", "Cinderstrike Hammer",
        "Stormweight Maul", "Deepmine Sledge", "Frostplate Hammer", "Shatterstone Maul",
        "Titan's Knocker", "Crownhammer", "Boltbreaker Maul", "Wyrmtooth Warhammer",
        "Ironfall Maul", "Earthshaker Sledge",
    ], 4, "Hammers.png")

    add_weapon_sheet([
        "Ashwood Quarterstaff", "Ironleaf Spear", "Blackforge Halberd", "Pilgrim's Staff", "Wayfarer's Staff",
        "Twinspike Partisan", "Skullthorn Polemace", "Tidefork Trident", "Stonecleaver Poleaxe",
        "Reedwind Quarterstaff", "Rivethead Polehammer", "Coldedge Halberd", "Sapphire Loop Staff",
        "Monk's Longstaff", "Hearthoak Staff", "Windcutter Glaive", "Starbreaker Warstar",
        "Ironhook Poleaxe", "Wanderwood Staff", "Hillpoint Spear", "Greywing Halberd",
        "Birch Quarterstaff", "Simplewood Staff", "Guard's Warstaff", "Flintfang Polemace",
        "Splitbeard Halberd", "Quarryreap Poleaxe", "Swiftthorn Javelin", "Oxblade Bardiche",
        "Reaper's Hookstaff", "Elm Quarterstaff", "Ashen Longstaff", "Lanternbearer Staff",
        "Bonecrush Polemace", "Warden's Trident", "Crowspike Warstar", "Azure-Studded Staff",
        "Royalcrest Halberd", "Ironcrown Polemace", "Nightwalk Staff", "Riveted Longstaff",
        "Spikefoot Pike", "Dreadwheel Warstar", "Anviljaw Poleaxe", "Blackbarb Bardiche",
        "Orchard Quarterstaff", "Dawnpoint Spear", "Longreach Pike", "Steelcapped Staff",
        "Arcflash Spellstaff", "Brawler's Quarterstaff", "Ragetooth Polemace", "Oathsplit Poleaxe",
        "Deepcut Poleaxe", "Willowwind Staff", "Frostpoint Spear", "Emberpoint Spear",
        "Blueflare Spellstaff", "Stoneweight Maulstaff", "Nailspike Maulstaff", "Stormcrown Polemace",
        "Riftedge Halberd", "Ironbranch Halberd", "Traveler's Staff", "Needlepoint Spear",
        "Boarward Winged Spear", "Dustwood Quarterstaff", "Knotroot Staff", "Moonglow Spellstaff",
        "Grudgeflange Polemace", "Frostwheel Warstar", "Duskcleave Poleaxe", "Creekwood Staff",
        "Skythrust Spear", "Bannerlord Halberd", "Cedar Longstaff", "Ironbark Warstaff",
        "Archmage's Crescent Staff", "Seawatch Trident", "Sunspike Warstar", "Titanbite Poleaxe",
        "Scholar's Staff", "Wolfpoint Spear", "Mooncrescent Glaive", "Pilgrim's Longstaff",
        "Ogresshoulder Maulstaff", "Serpentcoil Sorcerer Staff", "Stormfork Trident", "Gilded Oath Poleaxe",
        "Kingsplit Poleaxe",
    ], 9, "Polearms.png")

    add_weapon_sheet([
        "Ashwood War Scythe", "Rusthook Reaping Scythe", "Greyfang War Scythe", "Fieldhand Hand Scythe",
        "Pale Moon Crescent Scythe", "Bluejaw Razor Crescent Scythe", "Bronzewheat Harvest Scythe", "Rivet-Reap War Scythe",
        "Ironbriar Reaping Scythe", "Butcher's Hand Scythe", "Duskmarch Crescent Scythe", "Frostbite Sawtooth Crescent Scythe",
        "Longreach Harvest Scythe", "Hollowsteel War Scythe", "Blackthorn Reaping Scythe", "Crookblade Hand Scythe",
        "Gravewind Crescent Scythe", "Seawraith Razor Crescent Scythe", "Warden's War Scythe", "Stonecut Reaping Scythe",
        "Nightroot War Scythe", "Hooked Reaper Hand Scythe", "Murkmoon Crescent Scythe", "Stormcrescent Sawtooth Crescent Scythe",
        "Ironfield Harvest Scythe", "Anvilhook War Scythe", "Widowmaker Reaping Scythe", "Oathbreaker Hand Scythe",
        "Bonewhite Crescent Scythe", "Steelmoon Razor Crescent Scythe", "Ember-Reap War Scythe", "Forgebrand Reaping Scythe",
        "Shadowspike War Scythe", "Cindersickle Hand Scythe", "Sunken Arc Crescent Scythe", "Nightglass Sawtooth Crescent Scythe",
        "Frostfield Harvest Scythe", "Coldiron War Scythe", "Icefang Reaping Scythe", "Wintershear Hand Scythe",
        "Glacierhook Crescent Scythe", "Rimecrescent Razor Crescent Scythe", "Bloodharvest War Scythe", "Redhook Reaping Scythe",
        "Gorethorn War Scythe", "Bloodletter Hand Scythe", "Scarlet Arc Crescent Scythe", "Heartreap Sawtooth Crescent Scythe",
        "Voidreap War Scythe", "Ebonhook Reaping Scythe", "Riftfang War Scythe", "Umbral Hand Scythe",
        "Eclipse Crescent Scythe", "Starless Razor Crescent Scythe", "King's Harvest War Scythe", "Oathkeeper's Reaping Scythe",
        "Titanreaver War Scythe", "Archreaper Hand Scythe", "Seraph Arc Crescent Scythe", "Leviathan Sawtooth Crescent Scythe",
    ], 6, "Scythe.png")

    add_weapon_sheet([
        "Ashmark Arming Sword", "Ironridge Bastard Sword", "Fieldcleft Falchion", "Scout's Shortsword",
        "Squire's Longsword", "Suncrest Longsword", "Quarry Greatsword", "Gilded Zweihander",
        "Cleaveredge Greatsword", "Needlepoint Rapier", "Ragetooth Serrated Sword", "Seamsteel Backsword",
        "Harbor Sabre", "Duneshade Scimitar", "Crescent Khopesh", "Riverguard Arming Sword",
        "Copperbane Bastard Sword", "Stonecut Falchion", "Wayknife Shortsword", "Banneret Longsword",
        "Lioncross Longsword", "Hearthsplit Greatsword", "Crownwarden Zweihander", "Slabbreaker Greatsword",
        "Courtier Rapier", "Sawjaw Serrated Sword", "Greyline Backsword", "Privateer Sabre",
        "Sandrunner Scimitar", "Pharaoh's Khopesh", "Watchman Arming Sword", "Steelhand Bastard Sword",
        "Wolfbite Falchion", "Sentinel Shortsword", "Knightfall Longsword", "Brightsteel Longsword",
        "Bridgeguard Greatsword", "Regal Dawn Zweihander", "Headsman Greatsword", "Glassneedle Rapier",
        "Gnasher Serrated Sword", "Ironspine Backsword", "Seabreeze Sabre", "Mirage Scimitar",
        "Sunscar Khopesh", "Emberwake Arming Sword", "Cindergrip Bastard Sword", "Firedusk Falchion",
        "Ashpoint Shortsword", "Flameward Longsword", "Pyrekeeper Longsword", "Cindermaul Greatsword",
        "Phoenixcrest Zweihander", "Furnacecleave Greatsword", "Emberthread Rapier", "Charfang Serrated Sword",
        "Coalback Backsword", "Redtide Sabre", "Scorchwind Scimitar", "Ashen Khopesh",
        "Frostveil Arming Sword", "Rimeclad Bastard Sword", "Icehook Falchion", "Snowstep Shortsword",
        "Winterguard Longsword", "Glacierbrand Longsword", "Coldhammer Greatsword", "Frostcrown Zweihander",
        "Icebreaker Greatsword", "Silverfrost Rapier", "Rime-tooth Serrated Sword", "Coldspine Backsword",
        "Whitecap Sabre", "Sleetcurve Scimitar", "Polar Khopesh", "Stormline Arming Sword",
        "Thunderhand Bastard Sword", "Lightning Falchion", "Tempest Shortsword", "Stormwarden Longsword",
        "Skyforged Longsword", "Thunderclap Greatsword", "Stormking Zweihander", "Boltcleaver Greatsword",
        "Stormalis Rapier", "Shocktooth Serrated Sword", "Tempestspine Backsword", "Squall Sabre",
        "Sandstorm Scimitar", "Tempest Khopesh", "Duskhollow Arming Sword", "Nightguard Bastard Sword",
        "Umbral Falchion", "Shadowstep Shortsword", "Blackwatch Longsword", "Moonlit Longsword",
        "Nightfall Greatsword", "Eclipse Zweihander", "Gravecleave Greatsword", "Midnight Rapier",
        "Darktooth Serrated Sword", "Nightspine Backsword", "Nocturne Sabre", "Nightwind Scimitar",
        "Eclipse Khopesh", "Bloodmark Arming Sword", "Goregrip Bastard Sword", "Redreap Falchion",
        "Bloodedge Shortsword", "Crimson Longsword", "Hemlock Longsword", "Bloodhammer Greatsword",
        "Warlord Zweihander", "Butcher Greatsword", "Scarlet Rapier", "Ripper Serrated Sword",
        "Redspine Backsword", "Corsair Sabre", "Reddune Scimitar", "Bloodmoon Khopesh",
        "Voidkiss Arming Sword", "Riftbound Bastard Sword", "Nether Falchion", "Voidpoint Shortsword",
        "Starless Longsword", "Astral Longsword", "Blackhole Greatsword", "Voidcrown Zweihander",
        "Riftcleaver Greatsword", "Starneedle Rapier", "Nullfang Serrated Sword", "Voidspine Backsword",
        "Phantom Sabre", "Voidwind Scimitar", "Starfall Khopesh", "Dragonseal Arming Sword",
        "Titanhand Bastard Sword", "Kingsplit Falchion", "Royal Shortsword", "Oathkeeper Longsword",
        "Highlord Longsword", "Titanbreaker Greatsword", "Emperor's Zweihander", "Colossus Greatsword",
        "Grandduke Rapier", "Kingsaw Serrated Sword", "Crownspine Backsword", "Admiral Sabre",
        "Sultan's Scimitar", "Dynasty Khopesh",
    ], 15, "Swords.png")

    # Arrows.png (5x1) -> Ammo
    arrow_names = ["Wooden Arrow", "Iron Arrow", "Diamond Arrow", "Emerald Arrow", "Sunsteel Arrow"]
    arrow_tiers = ["Common", "Uncommon", "Epic", "Epic", "Legendary"]
    for c, nm in enumerate(arrow_names):
        tier, base, pool, max_aff = accessory_base("ammo", arrow_tiers[c], nm)
        add_item(items, nm, "Ammo", tier, "Arrows.png", 0, c, base, pool, max_affixes_for_rarity(tier, max_aff),
                 sockets_max=sockets_for("Ammo", tier))

    # Scroll.png (single) -> Trinket
    tier, base, pool, max_aff = accessory_base("trinket", "Rare", "Scroll")
    base = stats(flat={"cooldownReduction": 0.06, "rarityScore": 0.5})
    add_item(items, "Scroll", "Trinket", "Rare", "Scrolls.png", 0, 0, base, pool, max_affixes_for_rarity("Rare", max_aff),
             sockets_max=sockets_for("Trinket", "Rare"))

    # Scrolls.png (scripted consumables)
    scrolls = [
        ("scroll_invisibility", "Scroll of Invisibility", 0, 3, "Rare"),
        ("scroll_fireball", "Scroll of Fireball", 0, 6, "Uncommon"),
        ("scroll_summon_zombie", "Scroll of Summon Zombie", 3, 1, "Epic"),
        ("scroll_revive", "Scroll of Revive", 0, 4, "Epic"),
        ("scroll_rage", "Scroll of Rage", 0, 5, "Uncommon"),
        ("scroll_mirroring", "Scroll of Mirroring", 0, 8, "Rare"),
        ("scroll_lightning_prison", "Scroll of Lightning Prison", 1, 2, "Rare"),
        ("necronomicon", "Necronomicon", 4, 4, "Legendary"),
        ("scroll_flame_wall", "Scroll of Flame Wall", 4, 0, "Rare"),
        ("scroll_phase_step", "Scroll of Phase Step", 0, 7, "Uncommon"),
        ("scroll_sacrifice", "Scroll of Sacrifice", 5, 7, "Epic"),
        ("scroll_cataclysm", "Scroll of Cataclysm", 7, 5, "Legendary"),
    ]
    for cid, nm, r, c, tier in scrolls:
        add_item(items, nm, "Consumable", tier, "Scrolls.png", r, c, stats(), [], 0, consumable_id=cid)

    # UniqueGear.png (unique gear drops)
    unique_rows = [
        ["Knight's Helmet", "Viking Helmet", "Roman Helmet", "Roman Helmet", "Roman Helmet", "Warlock Hood", "Knight's Chainmail Hood", "Golden Crown"],
        ["Squire Armor", "Knight's Armor", "Barbarian Armor", "Viking Armor", "Gladiator Armor", "", "", ""],
        ["Death Stompers", "Silvertoe Boots", "Ranger's Gloves", "King's Crown", "Cloak of Urgency", "Cloak of Shadow", "Cloak of the Dead", ""],
        ["Ranger's Hood", "Chain Hood", "Gladiator Helm", "Templar Helmet", "Subjucated Crown", "Diamond Helmet", "", ""],
        ["Glowing Boots of Nimbleness", "Knights's Boots", "Gladiator's Boots", "Templar's Boots", "King's Boots", "Diamond Boots", "", ""],
        ["Knight Hip Guard", "Leather Hip Guard", "Templar Leggings", "King's Leggings", "Diamond Leggings", "", "", ""],
        ["Roman Chestplate", "Knight's Chestplate", "Gladiator's Chestplate", "Templar's Chestplate", "King's Chestplate", "Diamond Chestplate", "", ""],
    ]

    def unique_slot(name: str) -> str:
        nm = name.lower()
        if "cloak" in nm or "cape" in nm:
            return "Cloak"
        if "glove" in nm:
            return "Gloves"
        if "boot" in nm or "stomper" in nm:
            return "Boots"
        if "legging" in nm or "hip guard" in nm:
            return "Legs" if "legging" in nm else "Belt"
        if "armor" in nm or "chestplate" in nm or "chainmail" in nm:
            return "Chest"
        if "hood" in nm or "helmet" in nm or "helm" in nm or "crown" in nm:
            return "Head"
        return "Chest"

    for r, row_names in enumerate(unique_rows):
        for c, nm in enumerate(row_names):
            if not nm:
                continue
            slot = unique_slot(nm)
            _tier, base, pool, max_aff = armor_base(slot, "Sunsteel", nm) if slot in {"Head", "Chest", "Legs", "Boots", "Gloves", "Belt", "Cloak"} else (  # pragma: no cover - slot guarded
                "Legendary", stats(), [], 0
            )
            unique_base = scale_stats(base, flat_mul=2.5, mult_mul=1.5)
            unique_max_aff = max_affixes_for_unique(max_aff)
            add_item(items, nm, slot, "Unique", "UniqueGear.png", r, c, unique_base, pool, unique_max_aff,
                     sockets_max=sockets_for(slot, "Unique"))

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
    sizes = [("Small", 0.25, "Common"), ("Large", 0.50, "Common"), ("Giant", 0.75, "Common")]
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
