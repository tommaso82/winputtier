#include "i18n.h"
#include <windows.h>
#include <algorithm>
#include <cwchar>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wp {
namespace {
bool english=false;
constexpr wchar_t settingsKey[]=L"Software\\WinPuttier";
using Pair=std::pair<std::wstring,std::wstring>;
const std::unordered_map<std::wstring,std::wstring>& dictionary(){
 static const std::unordered_map<std::wstring,std::wstring> translations={
  {L"PuTTY Theme Manager  ·  Libreria offline  ·  Versione 0.5.1",L"PuTTY Theme Manager  ·  Offline library  ·  Version 0.5.1"},
  {L"01   SESSIONI PuTTY",L"01   PuTTY SESSIONS"},
  {L"02   LIBRERIA TEMI",L"02   THEME LIBRARY"},
  {L"03   ANTEPRIMA ANSI · SCORRI PER ESPLORARE",L"03   ANSI PREVIEW · SCROLL TO EXPLORE"},
  {L"EDITOR PALETTE    ·    22 colori",L"PALETTE EDITOR    ·    22 colours"},
  {L"RICERCA TEMI",L"SEARCH THEMES"},
  {L"Libreria locale",L"Local library"},
  {L"Digita il nome del tema...",L"Type a theme name..."},
  {L"Rileggi sessioni",L"Refresh sessions"},
  {L"Crea tema dalla sessione",L"Create theme from session"},
  {L"Applica tema",L"Apply theme"},
  {L"Ripristina colori originali",L"Restore original colours"},
  {L"Sincronizza libreria",L"Sync library"},
  {L"Gestisci fonti",L"Manage sources"},
  {L"Importa .reg",L"Import .reg"},
  {L"☆ Aggiungi preferito",L"☆ Add favourite"},
  {L"★ Rimuovi preferito",L"★ Remove favourite"},
  {L"Seleziona uno dei 22 colori",L"Select one of the 22 colours"},
  {L"Seleziona un tema disponibile",L"Select an available theme"},
  {L"I colori si modificano senza cambiare PuTTY.",L"Editing colours doesn't change PuTTY."},
  {L"Conferma HEX",L"Apply HEX"},
  {L"Selettore RGB",L"RGB picker"},
  {L"Annulla",L"Cancel"},
  {L"Nome del nuovo tema...",L"New theme name..."},
  {L"Salva personale",L"Save custom theme"},
  {L"Esporta .reg",L"Export .reg"},
  {L"Pronto",L"Ready"},
  {L"Tutte",L"All"},
  {L"Integrato",L"Built-in"},
  {L"Personali",L"Personal"},
  {L"Archivio locale",L"Local archive"},
  {L"★ Preferiti",L"★ Favourites"},
  {L"Testo",L"Text"},
  {L"Testo grassetto",L"Bold text"},
  {L"Sfondo",L"Background"},
  {L"Sfondo grassetto",L"Bold background"},
  {L"Testo cursore",L"Cursor text"},
  {L"Cursore",L"Cursor"},
  {L"Nero",L"Black"},
  {L"Nero brillante",L"Bright black"},
  {L"Rosso",L"Red"},
  {L"Rosso brillante",L"Bright red"},
  {L"Verde",L"Green"},
  {L"Verde brillante",L"Bright green"},
  {L"Giallo",L"Yellow"},
  {L"Giallo brillante",L"Bright yellow"},
  {L"Blu",L"Blue"},
  {L"Blu brillante",L"Bright blue"},
  {L"Magenta",L"Magenta"},
  {L"Magenta brillante",L"Bright magenta"},
  {L"Ciano",L"Cyan"},
  {L"Ciano brillante",L"Bright cyan"},
  {L"Bianco",L"White"},
  {L"Bianco brillante",L"Bright white"},
  {L"Temi PuTTY (*.reg)",L"PuTTY themes (*.reg)"},
  {L"Tutti i file (*.*)",L"All files (*.*)"},
  {L"Colore non valido",L"Invalid colour"},
  {L"Conferma applicazione tema",L"Confirm applying theme"},
  {L"Configurazione non valida",L"Invalid configuration"},
  {L"Una fonte per riga: Nome = link repository, cartella o file .reg su GitHub",L"One source per line: Name = GitHub repository, folder or .reg file URL"},
  {L"Salva fonti",L"Save sources"},
  {L"WinPuttier - Gestisci fonti",L"WinPuttier - Manage sources"},
  {L"01  TERMINALE / USCITA LINUX",L"01  LINUX TERMINAL OUTPUT"},
  {L"Output dimostrativo con colori ANSI, non una sessione SSH reale.",L"Sample ANSI output, not a real SSH session."},
  {L"02  DEFAULT, GRASSETTO E CURSORE",L"02  DEFAULT, BOLD AND CURSOR"},
  {L"Grassetto",L"Bold"},
  {L"Sfondo bold",L"Bold background"},
  {L"Grassetto: ",L"Bold: "},
  {L"solo font",L"font only"},
  {L"colore intenso",L"bright colour"},
  {L"font + colore intenso",L"font + bright colour"},
  {L"ATTENZIONE: PuTTY usa colori di sistema per i default/cursore.",L"WARNING: PuTTY uses system colours for default/cursor."},
  {L"Colori di testo, sfondo e cursore letti dalla palette.",L"Text, background and cursor colours are read from the palette."},
  {L"ANSI disattivato nella sessione: i blocchi sotto sono solo riferimenti.",L"ANSI disabled for this session: swatches below are reference only."},
  {L"03  ANSI 16 COLORI / CODICI SGR",L"03  ANSI 16 COLOURS / SGR CODES"},
  {L"04  TESTO x SFONDI ANSI NORMALI",L"04  TEXT x NORMAL ANSI BACKGROUNDS"},
  {L"05  TESTO x SFONDI ANSI BRILLANTI",L"05  TEXT x BRIGHT ANSI BACKGROUNDS"},
  {L"06  COME INTERPRETARE IL TEST",L"06  HOW TO READ THE TEST"},
  {L"Ogni cella mostra testo ANSI (riga) su sfondo ANSI (colonna).",L"Each cell shows ANSI text (row) on ANSI background (column)."},
  {L"RGB esatti Colour0..Colour21: nessuna correzione automatica.",L"Exact RGB values Colour0..Colour21: no automatic correction."},
  {L"Una combinazione poco leggibile qui lo sara' anche in PuTTY,",L"A hard-to-read combination here will also be hard to read in PuTTY,"},
  {L"salvo impostazioni come colori di sistema o grassetto.",L"unless settings such as system colours or bold change it."},
  {L"Scartare le modifiche non salvate della palette?",L"Discard unsaved palette changes?"},
  {L"Tema solo online. Usa Sincronizza libreria per ottenere tutti i file .reg.",L"This theme is not cached. Sync the library to download .reg files."},
  {L"Palette modificata solo nell'anteprima. Salva personale o applica alla sessione.",L"Palette changed in preview only. Save it or apply it to a session."},
  {L"Inserisci un valore HEX valido, ad esempio #7AA2F7.",L"Enter a valid HEX value, for example #7AA2F7."},
  {L"Modifiche alla palette annullate. Nessuna sessione PuTTY modificata.",L"Palette edits discarded. No PuTTY session was changed."},
  {L"Inserisci prima il nome del tema personale.",L"Enter a name for the custom theme first."},
  {L"Esiste gia' un tema personale con questo nome. Sovrascriverlo?",L"A custom theme with this name already exists. Overwrite it?"},
  {L"Seleziona prima la sessione a cui destinare il file .reg.",L"Select the session for the exported .reg file first."},
  {L"Impossibile scrivere il file .reg.",L"Could not write the .reg file."},
  {L"Seleziona una sessione e un tema disponibile.",L"Select a session and an available theme."},
  {L"Sincronizzazione raccolte: scaricamento dei soli file .reg ...",L"Synchronizing collections: downloading .reg files only..."},
  {L"Impossibile aprire l'editor fonti.",L"Could not open the source editor."},
  {L"Fonti aggiornate. Premi Sincronizza libreria per aggiornare i file .reg.",L"Sources updated. Click Sync library to update the .reg files."},
  {L"Non riesco a creare la cartella dati di WinPuttier.",L"Could not create the WinPuttier data directory."},
  {L"Seleziona la sessione a sinistra e il tema al centro; l'anteprima non modifica PuTTY.",L"Select a session on the left and a theme in the middle; previewing doesn't change PuTTY."},
  {L"Sessioni PuTTY ricaricate.",L"PuTTY sessions refreshed."},
  {L"Impossibile salvare i preferiti nella cartella locale.",L"Could not save favourites to the local folder."},
  {L"Tema rimosso dai preferiti.",L"Theme removed from favourites."},
  {L"Tema aggiunto ai preferiti.",L"Theme added to favourites."},
  {L"URL HTTPS non valido",L"Invalid HTTPS URL"},
  {L"WinHTTP non disponibile",L"WinHTTP unavailable"},
  {L"Impossibile connettersi a GitHub",L"Could not connect to GitHub"},
  {L"Richiesta HTTP fallita",L"HTTP request failed"},
  {L"Errore lettura HTTPS",L"HTTPS read error"},
  {L"Risposta troppo grande",L"Response too large"},
  {L"Il file non contiene tutti i 22 colori validi.",L"The file does not contain all 22 valid colours."},
  {L"Nessuna fonte disponibile: ",L"No available sources: "},
  {L"Manca la sezione [Sources]",L"Missing [Sources] section"},
  {L"Limite di 30 fonti",L"Maximum of 30 sources"},
  {L"Impossibile creare la cartella di configurazione.",L"Could not create the configuration folder."},
  {L"Impossibile scrivere le fonti.",L"Could not write the source configuration."},
  {L"Impossibile salvare sources.ini",L"Could not save sources.ini"},
  {L"Errore scrittura catalogo",L"Could not write the catalogue"},
  {L"Impossibile aggiornare il catalogo",L"Could not update the catalogue"},
  {L"Il tema non ha un URL",L"Theme has no URL"},
  {L"La fonte non contiene una palette PuTTY valida",L"The source does not contain a valid PuTTY palette"},
  {L"Impossibile creare le cartelle locali.",L"Could not create the local folders."},
  {L"Il catalogo non contiene alcun tema.",L"The catalogue contains no themes."},
  {L"Sessione non accessibile",L"Session inaccessible"},
  {L"Impossibile leggere i colori della sessione.",L"Could not read session colours."},
  {L"Salvataggio del backup originale fallito; nessuna modifica effettuata.",L"Could not save the original backup; nothing was changed."},
  {L"Impossibile leggere i colori correnti; nessuna modifica.",L"Could not read current colours; nothing was changed."},
  {L"Backup precedente non valido; nessuna modifica.",L"Invalid previous backup; nothing was changed."},
  {L"Il backup originale esistente non e' valido. Nessuna modifica effettuata.",L"Existing original backup is invalid. Nothing was changed."},
  {L"Scrittura fallita; colori precedenti ripristinati.",L"Write failed; previous colours restored."},
  {L"Backup originale assente o non valido per questa sessione.",L"Original backup is missing or invalid for this session."},
  {L"Impossibile leggere i colori attuali.",L"Could not read current colours."},
  {L"Ripristino fallito; configurazione precedente conservata.",L"Restore failed; previous configuration preserved."}
 };
 return translations;
}
const std::vector<Pair>& substitutions(){
 static const std::vector<Pair> phrases=[]{
  std::vector<Pair> items={
    {L" temi disponibili offline · cerca o filtra per raccolta",L" themes available offline · search or filter by collection"},
    {L"Colore ",L"Colour "},
    {L" (modificato)",L" (edited)"},
    {L"Tema personale salvato: ",L"Custom theme saved: "},
    {L". Nessuna modifica alle sessioni PuTTY.",L". No PuTTY sessions were changed."},
    {L"Tema esportato per ",L"Theme exported for "},
    {L". NON applicato automaticamente.",L". NOT applied automatically."},
    {L"Applicare il tema '",L"Apply theme '"},
    {L"' alla sessione '",L"' to session '"},
    {L"'?\n\nIl backup dei colori originali viene preservato. Nessun'altra sessione sara' modificata.",L"'?\n\nThe original colour backup is preserved. No other session will be modified."},
    {L"Ripristinare i colori ORIGINALI di '",L"Restore the ORIGINAL colours of '"},
    {L"'?\nIl backup originale rimarra' conservato.",L"'?\nThe original backup will be retained."},
    {L"Sincronizzazione file .reg: ",L"Syncing .reg files: "},
    {L"; WinPuttier - raccolte temi (modificabili)",L"; WinPuttier - editable theme collections"},
    {L"; Formato: Nome = link GitHub di repository/cartella/file .reg",L"; Format: Name = GitHub repository/folder/.reg file URL"},
    {L"Sessione non accessibile",L"Session inaccessible"},
    {L"Impossibile connettersi a GitHub",L"Could not connect to GitHub"},
    {L"Fonte non valida alla riga ",L"Invalid source on line "},
    {L"Nome o URL non valido alla riga ",L"Invalid name or URL on line "},
    {L"Nome fonte duplicato: ",L"Duplicate source name: "},
    {L"Fonti salvate: ",L"Sources saved: "},
    {L"Catalogo aggiornato: ",L"Catalogue updated: "},
    {L" temi.",L" themes."},
    {L" Alcune fonti non disponibili: ",L" Some sources unavailable: "},
    {L"Sincronizzazione annullata: ",L"Sync cancelled: "},
    {L"Temi: ",L"Themes: "},
    {L" | scaricati/aggiornati: ",L" | downloaded/updated: "},
    {L" | invariati: ",L" | unchanged: "},
    {L" | errori: ",L" | errors: "},
    {L" Attenzione: indice sincronizzazione non salvato.",L" Warning: sync index not saved."},
    {L"Tema importato: ",L"Theme imported: "},
    {L"Esiste gia' un tema con questo nome. Rinomina il file prima di importarlo.",L"A theme with this name already exists. Rename the file before importing."},
    {L"Impossibile creare il backup originale (errore ",L"Could not create original backup (error "},
    {L"Impossibile preparare il file .reg",L"Could not prepare the .reg file"},
    {L"Impossibile salvare il file .reg",L"Could not save the .reg file"},
    {L"Valore RGB non valido nella sessione: Colour",L"Invalid RGB value in session: Colour"},
    {L"ERRORE: ripristino parziale. Backup originale: ",L"ERROR: partial restore. Original backup: "},
    {L"Tema applicato a ",L"Theme applied to "},
    {L". Il backup originale e' conservato.",L". Original backup preserved."},
    {L"Ripristinati i colori ORIGINALI di ",L"Restored ORIGINAL colours for "},
    {L". Il backup originale rimane disponibile.",L". Original backup remains available."},
    {L" (errore Windows ",L" (Windows error "},
    {L"Errore connessione HTTPS (",L"HTTPS connection error ("},
    {L"GitHub HTTP ",L"GitHub HTTP "},
    {L" (eventuale limite API)",L" (possible API rate limit)"}
  };
  std::stable_sort(items.begin(),items.end(),[](const Pair& a,const Pair& b){
   return a.first.size()>b.first.size();
  });
  return items;
 }();
 return phrases;
}
} // namespace

void initLanguage(){
 english=PRIMARYLANGID(GetUserDefaultUILanguage())!=LANG_ITALIAN;
 HKEY key=nullptr;
 if(RegOpenKeyExW(HKEY_CURRENT_USER,settingsKey,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return;
 wchar_t stored[16]{};
 DWORD size=sizeof(stored),type=0;
 if(RegQueryValueExW(key,L"Language",nullptr,&type,
       reinterpret_cast<LPBYTE>(stored),&size)==ERROR_SUCCESS &&
    type==REG_SZ && size<=sizeof(stored) && size>=2*sizeof(wchar_t)){
   stored[15]=L'\0';
   if(wcscmp(stored,L"en")==0)english=true;
   else if(wcscmp(stored,L"it")==0)english=false;
 }
 RegCloseKey(key);
}
void setEnglish(bool enabled){
 english=enabled;
 HKEY key=nullptr;
 if(RegCreateKeyExW(HKEY_CURRENT_USER,settingsKey,0,nullptr,0,
                     KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return;
 const wchar_t* value=english?L"en":L"it";
 RegSetValueExW(key,L"Language",0,REG_SZ,
     reinterpret_cast<const BYTE*>(value),
     static_cast<DWORD>((wcslen(value)+1)*sizeof(wchar_t)));
 RegCloseKey(key);
}
bool isEnglish(){return english;}
std::wstring translate(const std::wstring& original,bool useEnglish){
 if(!useEnglish||original.empty())return original;
 const auto& found=dictionary();
 if(auto iter=found.find(original);iter!=found.end())return iter->second;
 std::wstring out=original;
 for(const auto& [from,to]:substitutions()){
  size_t start=0;
  while((start=out.find(from,start))!=std::wstring::npos){
    out.replace(start,from.size(),to);
    start+=to.size();
  }
 }
 return out;
}
std::wstring tr(const std::wstring& original){return translate(original,english);}
} // namespace wp
