#include "I18n.hpp"

namespace {

const i18n::Entry entries[] = {
    {"Totem Helper", "Totem-Helfer"},
    {"Turns the totem animation off if you like, warns you the moment your offhand has no totem, with text, sound, flash or a screen border, and makes the totems in your hotbar easy to spot. Display only: it never moves items or clicks for you.",
     "Schaltet auf Wunsch die Totem-Animation ab, warnt dich sofort, wenn in deiner Offhand kein Totem ist, mit Text, Ton, Blitz oder Bildschirmrand, und hebt die Totems in deiner Hotbar hervor. Nur Anzeige: Es verschiebt nie Items und klickt nie für dich."},
    {"Totem animation", "Totem-Animation"},
    {"Warning when the offhand has no totem", "Warnung, wenn kein Totem in der Offhand ist"},
    {"Only during a fight", "Nur im Kampf"},
    {"Fight lasts after the last hit (s)", "Kampf dauert nach dem letzten Treffer (s)"},
    {"Only when you still have totems", "Nur wenn du noch Totems hast"},
    {"Message", "Nachricht"},
    {"No totem in offhand!", "Kein Totem in der Offhand!"},
    {"Show totems left", "Übrige Totems anzeigen"},
    {"Text height on screen", "Höhe des Texts auf dem Bildschirm"},
    {"Alert", "Warnton"},
    {"Error", "Fehlerton"},
    {"Ding", "Ding"},
    {"Repeat every (s, 0 = once)", "Wiederholen alle (s, 0 = einmal)"},
    {"Flash color", "Blitzfarbe"},
    {"Screen border", "Bildschirmrand"},
    {"Border thickness", "Randdicke"},
    {"Highlight totems in the hotbar", "Totems in der Hotbar hervorheben"},
    {"Highlight totems in the inventory", "Totems im Inventar hervorheben"},
    {"Highlight style", "Art der Hervorhebung"},
    {"Slot background", "Slot-Hintergrund"},
    {"Glow and frame", "Leuchten und Rahmen"},
    {"Highlight color", "Farbe der Hervorhebung"},
    {"Text", "Text"},
    {"Item Size", "Item-Größe"},
    {"Makes the item icons in your hotbar and inventories smaller or bigger, like GUI Scale just for items. Display only.",
     "Macht die Item-Icons in deiner Hotbar und in Inventaren kleiner oder größer, wie GUI Scale nur für Items. Nur Anzeige."},
    {"Item icon size", "Größe der Item-Icons"},
};

i18n::Table table(entries);

}
