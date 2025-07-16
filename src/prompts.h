#ifndef PROMPTS_H
#define PROMPTS_H

// --- Prompts for ChatGPT ---
const char* PROMPT_IMAGE_RECOGNITION = "Gebe in nur 1-2 Wörtern zurück, um was für ein Lebensmittel es sich auf dem Bild handelt.";
const char* PROMPT_ADD_PRODUCT = "(Trenne alle Wörter in deiner Antwort bitte ausschließlich mit Leerzeichen. Verwende keine Tabulatoren, Zeilenumbrüche, Bindestriche etc.) Sage in einem kurzen Satz, dass folgendes Produkt in den Kühlschrank gelegt wurde: ";
const char* PROMPT_REMOVE_PRODUCT = "(Trenne alle Wörter in deiner Antwort bitte ausschließlich mit Leerzeichen. Verwende keine Tabulatoren, Zeilenumbrüche, Bindestriche etc.) Sage in einem kurzen Satz, dass folgendes Produkt aus dem Kühlschrank genommen wurde: ";
const char* PROMPT_FRIDGE_OPEN_FOR_TOO_LONG = "Biep Biep Biep Biep Biep Biep! Der Kühlschrank ist seit mehr als 5 Minuten offen. Bitte schließe ihn schnellstmöglich, damit die Lebensmittel nicht verderben!";

#endif // PROMPTS_H