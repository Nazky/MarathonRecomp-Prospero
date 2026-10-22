#include <user/config.h>
#include <locale/locale.h>

/*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! LOCALISATION NOTES !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    - Ensure brand names are always presented on the same line.

      Correct:
      This is a string that contains a brand name like
      Xbox 360, which is one of the two consoles to have a port of
      SONIC THE HEDGEHOG.

      Incorrect:
      This is a string that contains a brand name like Xbox
      360, which is one of the two consoles to have a port of SONIC THE
      HEDGEHOG.

    - Ensure your locale is added in the correct order following the language enum.

      Correct:
      {
          { ELanguage::English,  "Example" },
          { ELanguage::Japanese, "Example" },
          { ELanguage::German,   "Example" },
          { ELanguage::French,   "Example" },
          { ELanguage::Spanish,  "Example" },
          { ELanguage::Italian,  "Example" }
      }

      Incorrect:
      {
          { ELanguage::English,  "Example" },
          { ELanguage::French,   "Example" },
          { ELanguage::Spanish,  "Example" },
          { ELanguage::German,   "Example" },
          { ELanguage::Italian,  "Example" },
          { ELanguage::Japanese, "Example" }
      }

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

std::unordered_map<std::string_view, std::unordered_map<ELanguage, std::string>> g_locale =
{
    {
        "Options_Header_Name",
        {
            { ELanguage::English,  "OPTIONS" },
            { ELanguage::Japanese, "OPTIONS" },
            { ELanguage::German,   "OPTIONEN" },
            { ELanguage::French,   "OPTIONS" },
            { ELanguage::Spanish,  "OPCIONES" },
            { ELanguage::Italian,  "OPZIONI" }
        }
    },
    {
        "Options_Category_System",
        {
            { ELanguage::English,  "System settings" },
            { ELanguage::Japanese, "システム設定" },
            { ELanguage::German,   "System-Einstellungen" },
            { ELanguage::French,   "Paramètres système" },
            { ELanguage::Spanish,  "Configuración del sistema" },
            { ELanguage::Italian,  "Impostazioni sistema" }
        }
    },
    {
        "Options_Desc_Category_System",
        {
            { ELanguage::English,  "Adjust system settings." },
            { ELanguage::Japanese, "システムの設定を変更します" },
            { ELanguage::German,   "Verändere System-Einstellungen." },
            { ELanguage::French,   "Modifier les paramètres système." },
            { ELanguage::Spanish,  "Ajustar la configuración del sistema." },
            { ELanguage::Italian,  "Modifica le impostazioni di sistema." }
        }
    },
    {
        "Options_Category_Input",
        {
            { ELanguage::English,  "Input settings" },
            { ELanguage::Japanese, "入力設定" },
            { ELanguage::German,   "Eingabe-Einstellungen" },
            { ELanguage::French,   "Paramètres d'entrée" },
            { ELanguage::Spanish,  "Configuración de entrada" },
            { ELanguage::Italian,  "Impostazioni input" }
        }
    },
    {
        "Options_Desc_Category_Input",
        {
            { ELanguage::English,  "Adjust input settings." },
            { ELanguage::Japanese, "入力の設定を変更します" },
            { ELanguage::German,   "Verändere Eingabe-Einstellungen." },
            { ELanguage::French,   "Modifier les paramètres d'entrée." },
            { ELanguage::Spanish,  "Ajustar la configuración de entrada." },
            { ELanguage::Italian,  "Modifica le impostazioni input." }
        }
    },
    {
        "Options_Category_Audio",
        {
            { ELanguage::English,  "Audio settings" },
            { ELanguage::Japanese, "オーディオ設定" },
            { ELanguage::German,   "Audio-Einstellungen" },
            { ELanguage::French,   "Paramètres audio" },
            { ELanguage::Spanish,  "Configuración de audio" },
            { ELanguage::Italian,  "Impostazioni audio" }
        }
    },
    {
        "Options_Desc_Category_Audio",
        {
            { ELanguage::English,  "Adjust audio settings." },
            { ELanguage::Japanese, "オーディオの設定を変更します" },
            { ELanguage::German,   "Verändere Audio-Einstellungen." },
            { ELanguage::French,   "Modifier les paramètres audio." },
            { ELanguage::Spanish,  "Ajustar la configuración de audio." },
            { ELanguage::Italian,  "Modifica le impostazioni audio." }
        }
    },
    {
        "Options_Category_Video",
        {
            { ELanguage::English,  "Video settings" },
            { ELanguage::Japanese, "ビデオ設定" },
            { ELanguage::German,   "Video-Einstellungen" },
            { ELanguage::French,   "Paramètres vidéo" },
            { ELanguage::Spanish,  "Configuración de vídeo" },
            { ELanguage::Italian,  "Impostazioni video" }
        }
    },
    {
        "Options_Desc_Category_Video",
        {
            { ELanguage::English,  "Adjust video settings." },
            { ELanguage::Japanese, "ビデオの設定を変更します" },
            { ELanguage::German,   "Verändere Video-Einstellungen." },
            { ELanguage::French,   "Modifier les paramètres vidéo." },
            { ELanguage::Spanish,  "Ajustar la configuración de vídeo." },
            { ELanguage::Italian,  "Modifica le impostazioni video." }
        }
    },
    {
        "Options_Category_Mods",
        {
            { ELanguage::English,  "Mods" },
            { ELanguage::Japanese, "Mods" },
            { ELanguage::German,   "Mods" },
            { ELanguage::French,   "Mods" },
            { ELanguage::Spanish,  "Mods" },
            { ELanguage::Italian,  "Mod" }
        }
    },
    {
        "Options_Desc_Category_Mods",
        {
            { ELanguage::English,  "A toggles a mod; left/right changes load order. Restart to apply." },
            { ELanguage::Japanese, "AでMODを切り替え、左右で読み込み順を変更します。適用には再起動が必要です" },
            { ELanguage::German,   "A schaltet Mods um; links/rechts ändert die Ladereihenfolge. Neustart zum Anwenden." },
            { ELanguage::French,   "A active ou désactive un mod ; gauche/droite change l'ordre. Redémarrez pour appliquer." },
            { ELanguage::Spanish,  "A activa o desactiva un mod; izquierda/derecha cambia el orden. Reinicia para aplicar." },
            { ELanguage::Italian,  "A attiva o disattiva una mod; sinistra/destra cambia l'ordine. Riavvia per applicare." }
        }
    },
    {
        "Options_Mods_Enabled",
        {
            { ELanguage::English,  "Enabled" },
            { ELanguage::Japanese, "有効" },
            { ELanguage::German,   "Aktiviert" },
            { ELanguage::French,   "Activé" },
            { ELanguage::Spanish,  "Activado" },
            { ELanguage::Italian,  "Attivata" }
        }
    },
    {
        "Options_Mods_Disabled",
        {
            { ELanguage::English,  "Disabled" },
            { ELanguage::Japanese, "無効" },
            { ELanguage::German,   "Deaktiviert" },
            { ELanguage::French,   "Désactivé" },
            { ELanguage::Spanish,  "Desactivado" },
            { ELanguage::Italian,  "Disattivata" }
        }
    },
    {
        "Options_Mods_Empty",
        {
            { ELanguage::English,  "No mods found. Add mod folders to:" },
            { ELanguage::Japanese, "MODが見つかりません。MODフォルダーを次の場所に追加してください:" },
            { ELanguage::German,   "Keine Mods gefunden. Mod-Ordner hinzufügen unter:" },
            { ELanguage::French,   "Aucun mod trouvé. Ajoutez des dossiers de mods dans :" },
            { ELanguage::Spanish,  "No se encontraron mods. Añade carpetas de mods en:" },
            { ELanguage::Italian,  "Nessuna mod trovata. Aggiungi le cartelle delle mod in:" }
        }
    },
    {
        "Options_Restart_Mods",
        {
            { ELanguage::English,  "Mod list / load order" },
            { ELanguage::Japanese, "MOD一覧 / 読み込み順" },
            { ELanguage::German,   "Mod-Liste / Ladereihenfolge" },
            { ELanguage::French,   "Liste / ordre des mods" },
            { ELanguage::Spanish,  "Lista / orden de mods" },
            { ELanguage::Italian,  "Elenco / ordine delle mod" }
        }
    },
    {
        "Mods_Title",
        {
            { ELanguage::English,  "MOD MANAGER" },
            { ELanguage::Japanese, "MOD MANAGER" },
            { ELanguage::German,   "MOD-VERWALTUNG" },
            { ELanguage::French,   "GESTION DES MODS" },
            { ELanguage::Spanish,  "GESTOR DE MODS" },
            { ELanguage::Italian,  "GESTIONE MOD" }
        }
    },
    {
        "Mods_Help_List",
        {
            { ELanguage::English,  "A toggles a mod. Left and right change load priority." },
            { ELanguage::Japanese, "AでMODを有効・無効にし、左右で読み込み順を変更します" },
            { ELanguage::German,   "A aktiviert oder deaktiviert Mods. Links und rechts ändern die Ladereihenfolge." },
            { ELanguage::French,   "A active ou désactive un mod. Gauche et droite changent l'ordre de chargement." },
            { ELanguage::Spanish,  "A activa o desactiva un mod. Izquierda y derecha cambian el orden de carga." },
            { ELanguage::Italian,  "A attiva o disattiva una mod. Sinistra e destra cambiano l'ordine di caricamento." }
        }
    },
    {
        "Mods_Help_Options",
        {
            { ELanguage::English,  "Up and down select an option. Left and right change its value." },
            { ELanguage::Japanese, "上下でオプションを選び、左右で値を変更します" },
            { ELanguage::German,   "Oben und unten wählen eine Option. Links und rechts ändern den Wert." },
            { ELanguage::French,   "Haut et bas sélectionnent une option. Gauche et droite changent sa valeur." },
            { ELanguage::Spanish,  "Arriba y abajo seleccionan una opción. Izquierda y derecha cambian su valor." },
            { ELanguage::Italian,  "Su e giù selezionano un'opzione. Sinistra e destra ne cambiano il valore." }
        }
    },
    {
        "Mods_Help_Order",
        {
            { ELanguage::English,  "Use left/right to move a mod earlier or later in load priority. The first enabled mod takes precedence." },
            { ELanguage::Japanese, "左右で読み込み順を変更します。最初の有効なMODが優先されます" },
            { ELanguage::German,   "Mit links/rechts wird die Ladereihenfolge geändert. Die erste aktivierte Mod hat Vorrang." },
            { ELanguage::French,   "Gauche/droite modifie l'ordre de chargement. Le premier mod activé est prioritaire." },
            { ELanguage::Spanish,  "Izquierda/derecha cambia el orden de carga. El primer mod activado tiene prioridad." },
            { ELanguage::Italian,  "Sinistra/destra cambia l'ordine di caricamento. La prima mod attiva ha la priorità." }
        }
    },
    {
        "Mods_Empty",
        {
            { ELanguage::English,  "No managed mods were found." },
            { ELanguage::Japanese, "管理対象のMODが見つかりません" },
            { ELanguage::German,   "Keine verwalteten Mods gefunden." },
            { ELanguage::French,   "Aucun mod géré n'a été trouvé." },
            { ELanguage::Spanish,  "No se encontraron mods administrados." },
            { ELanguage::Italian,  "Nessuna mod gestita trovata." }
        }
    },
    {
        "Mods_EmptyHint",
        {
            { ELanguage::English,  "Add one mod folder per installed mod here:" },
            { ELanguage::Japanese, "ここにMODごとのフォルダーを追加してください:" },
            { ELanguage::German,   "Füge hier für jede Mod einen Ordner hinzu:" },
            { ELanguage::French,   "Ajoutez ici un dossier pour chaque mod :" },
            { ELanguage::Spanish,  "Añade aquí una carpeta para cada mod:" },
            { ELanguage::Italian,  "Aggiungi qui una cartella per ogni mod:" }
        }
    },
    {
        "Mods_OptionsCount",
        {
            { ELanguage::English,  "options" },
            { ELanguage::Japanese, "オプション" },
            { ELanguage::German,   "Optionen" },
            { ELanguage::French,   "options" },
            { ELanguage::Spanish,  "opciones" },
            { ELanguage::Italian,  "opzioni" }
        }
    },
    {
        "Mods_NoOptions",
        {
            { ELanguage::English,  "This mod does not declare configurable options." },
            { ELanguage::Japanese, "このMODには変更可能なオプションがありません" },
            { ELanguage::German,   "Diese Mod stellt keine konfigurierbaren Optionen bereit." },
            { ELanguage::French,   "Ce mod ne déclare aucune option configurable." },
            { ELanguage::Spanish,  "Este mod no declara opciones configurables." },
            { ELanguage::Italian,  "Questa mod non dichiara opzioni configurabili." }
        }
    },
    {
        "Mods_ReadOnly",
        {
            { ELanguage::English,  "Read-only" },
            { ELanguage::Japanese, "読み取り専用" },
            { ELanguage::German,   "Schreibgeschützt" },
            { ELanguage::French,   "Lecture seule" },
            { ELanguage::Spanish,  "Solo lectura" },
            { ELanguage::Italian,  "Sola lettura" }
        }
    },
    {
        "Mods_Value_Default",
        {
            { ELanguage::English,  "Default" },
            { ELanguage::Japanese, "既定" },
            { ELanguage::German,   "Standard" },
            { ELanguage::French,   "Par défaut" },
            { ELanguage::Spanish,  "Predeterminado" },
            { ELanguage::Italian,  "Predefinito" }
        }
    },
    {
        "Mods_Value_Custom",
        {
            { ELanguage::English,  "Custom" },
            { ELanguage::Japanese, "カスタム" },
            { ELanguage::German,   "Benutzerdefiniert" },
            { ELanguage::French,   "Personnalisé" },
            { ELanguage::Spanish,  "Personalizado" },
            { ELanguage::Italian,  "Personalizzato" }
        }
    },
    {
        "Mods_Author",
        {
            { ELanguage::English,  "Author" },
            { ELanguage::Japanese, "作者" },
            { ELanguage::German,   "Autor" },
            { ELanguage::French,   "Auteur" },
            { ELanguage::Spanish,  "Autor" },
            { ELanguage::Italian,  "Autore" }
        }
    },
    {
        "Mods_NoDescription",
        {
            { ELanguage::English,  "No description provided." },
            { ELanguage::Japanese, "説明はありません" },
            { ELanguage::German,   "Keine Beschreibung vorhanden." },
            { ELanguage::French,   "Aucune description fournie." },
            { ELanguage::Spanish,  "No se proporcionó una descripción." },
            { ELanguage::Italian,  "Nessuna descrizione fornita." }
        }
    },
    {
        "Mods_Folder",
        {
            { ELanguage::English,  "Folder" },
            { ELanguage::Japanese, "フォルダー" },
            { ELanguage::German,   "Ordner" },
            { ELanguage::French,   "Dossier" },
            { ELanguage::Spanish,  "Carpeta" },
            { ELanguage::Italian,  "Cartella" }
        }
    },
    {
        "Mods_OptionCount",
        {
            { ELanguage::English,  "Configurable options" },
            { ELanguage::Japanese, "設定可能なオプション" },
            { ELanguage::German,   "Konfigurierbare Optionen" },
            { ELanguage::French,   "Options configurables" },
            { ELanguage::Spanish,  "Opciones configurables" },
            { ELanguage::Italian,  "Opzioni configurabili" }
        }
    },
    {
        "Mods_Installed",
        {
            { ELanguage::English,  "managed mods" },
            { ELanguage::Japanese, "管理対象MOD" },
            { ELanguage::German,   "verwaltete Mods" },
            { ELanguage::French,   "mods gérés" },
            { ELanguage::Spanish,  "mods administrados" },
            { ELanguage::Italian,  "mod gestite" }
        }
    },
    {
        "Mods_RestartChange",
        {
            { ELanguage::English,  "Mod enable state, load order, or configuration options" },
            { ELanguage::Japanese, "MODの有効状態、読み込み順、または設定オプション" },
            { ELanguage::German,   "Mod-Aktivierung, Ladereihenfolge oder Konfigurationsoptionen" },
            { ELanguage::French,   "Activation, ordre de chargement ou options de configuration des mods" },
            { ELanguage::Spanish,  "Activación, orden de carga u opciones de configuración de los mods" },
            { ELanguage::Italian,  "Attivazione, ordine di caricamento o opzioni di configurazione delle mod" }
        }
    },
    {
        "Mods_Message_RestartConfirmClose",
        {
            { ELanguage::English,  "Mod changes are staged. Save them and close the game now? Relaunch the game to apply them. Choosing No discards these changes." },
            { ELanguage::Japanese, "MODの変更はまだ保存されていません。変更を保存してゲームを終了しますか？反映するにはゲームを再起動してください。「いいえ」を選ぶと変更は破棄されます" },
            { ELanguage::German,   "Mod-Änderungen sind vorgemerkt. Jetzt speichern und das Spiel schließen? Starte das Spiel erneut, um sie anzuwenden. Nein verwirft die Änderungen." },
            { ELanguage::French,   "Les changements de mods sont en attente. Les enregistrer et fermer le jeu maintenant ? Relancez-le pour les appliquer. Non annule ces changements." },
            { ELanguage::Spanish,  "Los cambios de mods están pendientes. ¿Guardarlos y cerrar el juego ahora? Reinícialo para aplicarlos. No descarta los cambios." },
            { ELanguage::Italian,  "Le modifiche alle mod sono in attesa. Salvarle e chiudere il gioco ora? Riavvialo per applicarle. No annulla queste modifiche." }
        }
    },
    {
        "Mods_Error_SaveFailed",
        {
            { ELanguage::English,  "Could not save mod settings. Check the managed mod files and try again." },
            { ELanguage::Japanese, "MOD設定を保存できません。管理対象のMODファイルを確認して再試行してください" },
            { ELanguage::German,   "Mod-Einstellungen konnten nicht gespeichert werden. Prüfe die Mod-Dateien und versuche es erneut." },
            { ELanguage::French,   "Impossible d'enregistrer les paramètres des mods. Vérifiez les fichiers et réessayez." },
            { ELanguage::Spanish,  "No se pudieron guardar los ajustes de mods. Revisa los archivos y vuelve a intentarlo." },
            { ELanguage::Italian,  "Impossibile salvare le impostazioni delle mod. Controlla i file e riprova." }
        }
    },
    {
        "Mods_Button_Open",
        {
            { ELanguage::English,  "Mods" },
            { ELanguage::Japanese, "Mods" },
            { ELanguage::German,   "Mods" },
            { ELanguage::French,   "Mods" },
            { ELanguage::Spanish,  "Mods" },
            { ELanguage::Italian,  "Mod" }
        }
    },
    {
        "Mods_Button_Toggle",
        {
            { ELanguage::English,  "Toggle" },
            { ELanguage::Japanese, "切替" },
            { ELanguage::German,   "Umschalten" },
            { ELanguage::French,   "Activer" },
            { ELanguage::Spanish,  "Alternar" },
            { ELanguage::Italian,  "Attiva" }
        }
    },
    {
        "Mods_Button_Options",
        {
            { ELanguage::English,  "Options" },
            { ELanguage::Japanese, "オプション" },
            { ELanguage::German,   "Optionen" },
            { ELanguage::French,   "Options" },
            { ELanguage::Spanish,  "Opciones" },
            { ELanguage::Italian,  "Opzioni" }
        }
    },
    {
        "Mods_Button_Change",
        {
            { ELanguage::English,  "Change" },
            { ELanguage::Japanese, "変更" },
            { ELanguage::German,   "Ändern" },
            { ELanguage::French,   "Changer" },
            { ELanguage::Spanish,  "Cambiar" },
            { ELanguage::Italian,  "Cambia" }
        }
    },
    {
        "Mods_Error",
        {
            { ELanguage::English,  "Error" },
            { ELanguage::Japanese, "エラー" },
            { ELanguage::German,   "Fehler" },
            { ELanguage::French,   "Erreur" },
            { ELanguage::Spanish,  "Error" },
            { ELanguage::Italian,  "Errore" }
        }
    },
    {
        "SystemInfo_FPS",
        {
            { ELanguage::English,  "FPS" },
            { ELanguage::Japanese, "FPS" },
            { ELanguage::German,   "FPS" },
            { ELanguage::French,   "IPS" },
            { ELanguage::Spanish,  "FPS" },
            { ELanguage::Italian,  "FPS" }
        }
    },
    {
        "SystemInfo_CPU",
        {
            { ELanguage::English,  "CPU update" },
            { ELanguage::Japanese, "CPU更新" },
            { ELanguage::German,   "CPU-Update" },
            { ELanguage::French,   "CPU (jeu)" },
            { ELanguage::Spanish,  "CPU (juego)" },
            { ELanguage::Italian,  "CPU (gioco)" }
        }
    },
    {
        "SystemInfo_GPU",
        {
            { ELanguage::English,  "GPU" },
            { ELanguage::Japanese, "GPU" },
            { ELanguage::German,   "GPU" },
            { ELanguage::French,   "GPU" },
            { ELanguage::Spanish,  "GPU" },
            { ELanguage::Italian,  "GPU" }
        }
    },
    {
        "SystemInfo_GPUFrame",
        {
            { ELanguage::English,  "GPU frame" },
            { ELanguage::Japanese, "GPU時間" },
            { ELanguage::German,   "GPU-Frame" },
            { ELanguage::French,   "Temps GPU" },
            { ELanguage::Spanish,  "Tiempo GPU" },
            { ELanguage::Italian,  "Tempo GPU" }
        }
    },
    {
        "SystemInfo_SectionPerformance",
        {
            { ELanguage::English,  "PERFORMANCE" },
            { ELanguage::Japanese, "性能" },
            { ELanguage::German,   "LEISTUNG" },
            { ELanguage::French,   "PERFORMANCES" },
            { ELanguage::Spanish,  "RENDIMIENTO" },
            { ELanguage::Italian,  "PRESTAZIONI" }
        }
    },
    {
        "SystemInfo_SectionDisplay",
        {
            { ELanguage::English,  "DISPLAY" },
            { ELanguage::Japanese, "表示" },
            { ELanguage::German,   "ANZEIGE" },
            { ELanguage::French,   "AFFICHAGE" },
            { ELanguage::Spanish,  "PANTALLA" },
            { ELanguage::Italian,  "SCHERMO" }
        }
    },
    {
        "SystemInfo_Resolution",
        {
            { ELanguage::English,  "Render" },
            { ELanguage::Japanese, "描画" },
            { ELanguage::German,   "Render" },
            { ELanguage::French,   "Rendu" },
            { ELanguage::Spanish,  "Render" },
            { ELanguage::Italian,  "Render" }
        }
    },
    {
        "SystemInfo_FPSCap",
        {
            { ELanguage::English,  "FPS cap" },
            { ELanguage::Japanese, "FPS上限" },
            { ELanguage::German,   "FPS-Limit" },
            { ELanguage::French,   "Limite FPS" },
            { ELanguage::Spanish,  "Límite de FPS" },
            { ELanguage::Italian,  "Limite FPS" }
        }
    },
    {
        "SystemInfo_SectionSoCSensors",
        {
            { ELanguage::English,  "SOC SENSORS" },
            { ELanguage::Japanese, "SoCセンサー" },
            { ELanguage::German,   "SOC-SENSOREN" },
            { ELanguage::French,   "CAPTEURS SOC" },
            { ELanguage::Spanish,  "SENSORES SOC" },
            { ELanguage::Italian,  "SENSORI SOC" }
        }
    },
    {
        "SystemInfo_SoCSensor",
        {
            { ELanguage::English,  "SoC sensor" },
            { ELanguage::Japanese, "SoCセンサー" },
            { ELanguage::German,   "SoC-Sensor" },
            { ELanguage::French,   "Capteur SoC" },
            { ELanguage::Spanish,  "Sensor SoC" },
            { ELanguage::Italian,  "Sensore SoC" }
        }
    },
    {
        "SystemInfo_SectionMemory",
        {
            { ELanguage::English,  "MEMORY" },
            { ELanguage::Japanese, "メモリ" },
            { ELanguage::German,   "SPEICHER" },
            { ELanguage::French,   "MÉMOIRE" },
            { ELanguage::Spanish,  "MEMORIA" },
            { ELanguage::Italian,  "MEMORIA" }
        }
    },
    {
        "SystemInfo_GameHeap",
        {
            { ELanguage::English,  "Game RAM heap" },
            { ELanguage::Japanese, "ゲームRAM" },
            { ELanguage::German,   "Spiel-RAM/Heap" },
            { ELanguage::French,   "RAM du jeu" },
            { ELanguage::Spanish,  "RAM del juego" },
            { ELanguage::Italian,  "RAM di gioco" }
        }
    },
    {
        "SystemInfo_PhysicalHeap",
        {
            { ELanguage::English,  "Physical RAM heap" },
            { ELanguage::Japanese, "物理RAM" },
            { ELanguage::German,   "Physischer RAM-Heap" },
            { ELanguage::French,   "RAM physique" },
            { ELanguage::Spanish,  "RAM física" },
            { ELanguage::Italian,  "RAM fisica" }
        }
    },
    {
        "SystemInfo_SectionPaths",
        {
            { ELanguage::English,  "PATHS" },
            { ELanguage::Japanese, "パス" },
            { ELanguage::German,   "PFADE" },
            { ELanguage::French,   "CHEMINS" },
            { ELanguage::Spanish,  "RUTAS" },
            { ELanguage::Italian,  "PERCORSI" }
        }
    },
    {
        "SystemInfo_UserDataPath",
        {
            { ELanguage::English,  "User data" },
            { ELanguage::Japanese, "ユーザーデータ" },
            { ELanguage::German,   "Benutzerdaten" },
            { ELanguage::French,   "Données utilisateur" },
            { ELanguage::Spanish,  "Datos de usuario" },
            { ELanguage::Italian,  "Dati utente" }
        }
    },
    {
        "SystemInfo_ExecutablePath",
        {
            { ELanguage::English,  "Game executable" },
            { ELanguage::Japanese, "ゲーム実行ファイル" },
            { ELanguage::German,   "Spielexecutable" },
            { ELanguage::French,   "Exécutable du jeu" },
            { ELanguage::Spanish,  "Ejecutable del juego" },
            { ELanguage::Italian,  "Eseguibile del gioco" }
        }
    },
    {
        "SystemInfo_GameMount",
        {
            { ELanguage::English,  "Game mount" },
            { ELanguage::Japanese, "ゲームマウント" },
            { ELanguage::German,   "Spiel-Mount" },
            { ELanguage::French,   "Montage du jeu" },
            { ELanguage::Spanish,  "Montaje del juego" },
            { ELanguage::Italian,  "Mount del gioco" }
        }
    },
    {
        "SystemInfo_OutputResolution",
        {
            { ELanguage::English,  "Output" },
            { ELanguage::Japanese, "出力" },
            { ELanguage::German,   "Ausgabe" },
            { ELanguage::French,   "Sortie" },
            { ELanguage::Spanish,  "Salida" },
            { ELanguage::Italian,  "Output" }
        }
    },
    {
        "SystemInfo_RenderScale",
        {
            { ELanguage::English,  "Scale" },
            { ELanguage::Japanese, "倍率" },
            { ELanguage::German,   "Skalierung" },
            { ELanguage::French,   "Échelle" },
            { ELanguage::Spanish,  "Escala" },
            { ELanguage::Italian,  "Scala" }
        }
    },
    {
        "Options_Category_Debug",
        {
            { ELanguage::English,  "Debug settings" },
            { ELanguage::Japanese, "デバッグ設定" },
            { ELanguage::German,   "Debug-Einstellungen" },
            { ELanguage::French,   "Paramètres de débogage" },
            { ELanguage::Spanish,  "Configuración de depuración" },
            { ELanguage::Italian,  "Impostazioni debug" }
        }
    },
    {
        "Options_Desc_Category_Debug",
        {
            { ELanguage::English,  "Adjust debug settings." },
            { ELanguage::Japanese, "デバッグの設定を変更します" },
            { ELanguage::German,   "Verändere Debug-Einstellungen." },
            { ELanguage::French,   "Modifier les paramètres de débogage." },
            { ELanguage::Spanish,  "Ajustar la configuración de depuración." },
            { ELanguage::Italian,  "Modifica le impostazioni debug." }
        }
    },
    {
        // Notes: integer values in the options menu (e.g. FPS) when at their maximum value.
        "Options_Value_Max",
        {
            { ELanguage::English,  "MAX" },
            { ELanguage::Japanese, "MAX" },
            { ELanguage::German,   "MAX" },
            { ELanguage::French,   "MAX" },
            { ELanguage::Spanish,  "MÁX" },
            { ELanguage::Italian,  "MAX" }
        }
    },
    {
        "Options_Name_WindowSize",
        {
            { ELanguage::English,  "Window Size" },
            { ELanguage::Japanese, "ウィンドウサイズ" },
            { ELanguage::German,   "Fenstergröße" },
            { ELanguage::French,   "Taille de la fenêtre" },
            { ELanguage::Spanish,  "Tamaño de ventana" },
            { ELanguage::Italian,  "Dimensioni della finestra" }
        }
    },
    {
        "Options_Desc_WindowSize",
        {
            { ELanguage::English,  "Adjust the size of the game window in windowed mode." },
            { ELanguage::Japanese, "ゲームのウィンドウサイズを設定できます" },
            { ELanguage::German,   "Passe die Fenstergröße des Spiels im Fenstermodus an." },
            { ELanguage::French,   "Définir la résolution de jeu en mode fenêtré." },
            { ELanguage::Spanish,  "Ajusta el tamaño de la ventana de juego en modo ventana." },
            { ELanguage::Italian,  "Regola la dimensione della finestra del gioco in modalità finestra." }
        }
    },
    {
        // Notes: description for options that cannot be accessed during gameplay (e.g. Language).
        "Options_Desc_NotAvailable",
        {
            { ELanguage::English,  "This option is not available during gameplay." },
            { ELanguage::Japanese, "このオプションはゲームプレイ中は使用できません" },
            { ELanguage::German,   "Diese Einstellung kann während des Spiels nicht verändert werden." },
            { ELanguage::French,   "Cette option est indisponible en cours de jeu." },
            { ELanguage::Spanish,  "Esta opción no está disponible durante la partida." },
            { ELanguage::Italian,  "Questa opzione non è disponibile durante il gioco." }
        }
    },
    {
        // Notes: description for options that are not implemented yet in development builds.
        "Options_Desc_NotImplemented",
        {
            { ELanguage::English,  "This option is not implemented yet." },
            { ELanguage::Japanese, "このオプションはまだ実装されていません" },
            { ELanguage::German,   "Diese Einstellung wurde noch nicht implementiert." },
            { ELanguage::French,   "Cette option n'est pas encore implémentée." },
            { ELanguage::Spanish,  "Esta opción aún no está implementada." },
            { ELanguage::Italian,  "Questa opzione non è ancora implementata." }
        }
    },
    {
        // Notes: currently the description for Window Size when in fullscreen.
        "Options_Desc_NotAvailableFullscreen",
        {
            { ELanguage::English,  "This option is not available in fullscreen mode." },
            { ELanguage::Japanese, "このオプションはフルスクリーンモードでは変更できません" },
            { ELanguage::German,   "Diese Option ist im Vollbildmodus nicht verfügbar." },
            { ELanguage::French,   "Cette option n'est pas disponible en mode plein écran." },
            { ELanguage::Spanish,  "Esta opción no está disponible en modo pantalla completa." },
            { ELanguage::Italian,  "Questa opzione non è disponibile in modalità schermo pieno." }
        }
    },
    {
        // Notes: currently the description for Monitor when in fullscreen.
        "Options_Desc_NotAvailableWindowed",
        {
            { ELanguage::English,  "This option is not available in windowed mode." },
            { ELanguage::Japanese, "このオプションはウィンドウモードでは変更できません" },
            { ELanguage::German,   "Diese Option ist im Fenstermodus nicht verfügbar." },
            { ELanguage::French,   "Cette option n'est pas disponible en mode fenêtré." },
            { ELanguage::Spanish,  "Esta opción no está disponible en modo ventana." },
            { ELanguage::Italian,  "Questa opzione non è disponibile in modalità finestra." }
        }
    },
    {
        // Notes: currently the description for Monitor when the user only has one display connected.
        "Options_Desc_NotAvailableHardware",
        {
            { ELanguage::English,  "This option is not available with your current hardware configuration." },
            { ELanguage::Japanese, "このオプションは現在のハードウェア構成で変更できません" },
            { ELanguage::German,   "Diese Option ist mit der momentanen Hardwarekonfiguration nicht verfügbar." },
            { ELanguage::French,   "Cette option n'est pas disponible avec votre configuration matérielle actuelle." },
            { ELanguage::Spanish,  "Esta opción no está disponible con tu configuración actual de hardware." },
            { ELanguage::Italian,  "Questa opzione non è disponibile con l'hardware in tuo possesso." }
        }
    },
    {
        // Notes: description for Transparency Anti-Aliasing when MSAA is disabled.
        "Options_Desc_NotAvailableMSAA",
        {
            { ELanguage::English,  "This option is not available without MSAA." },
            { ELanguage::Japanese, "このオプションはMSAAなしで変更できません" },
            { ELanguage::German,   "Diese Option ist ohne MSAA nicht verfügbar." },
            { ELanguage::French,   "Cette option n'est pas disponible sans MSAA." },
            { ELanguage::Spanish,  "Esta opción no está disponible sin MSAA." },
            { ELanguage::Italian,  "Questa opzione non è disponibile senza MSAA." }
        }
    },
    {
        // Notes: description for Music Attenuation when the user is not running a supported OS.
        "Options_Desc_OSNotSupported",
        {
            { ELanguage::English,  "This option is not supported by your operating system." },
            { ELanguage::Japanese, "このオプションは現在のOSで変更できません" },
            { ELanguage::German,   "Diese Option wird von diesem Betriebssystem nicht unterstützt." },
            { ELanguage::French,   "Cette option n'est pas prise en charge par votre système d'exploitation." },
            { ELanguage::Spanish,  "Esta opción no es compatible con tu sistema operativo." },
            { ELanguage::Italian,  "Questa opzione non è disponibile con il tuo sistema operativo." }
        }
    },
    {
        "Options_Message_Restart",
        {
            { ELanguage::English,  "The game needs to restart to apply\nthe following changes. OK?\n" },
            { ELanguage::Japanese, "以下の変更を適用するには\nゲームを再起動する必要があります\nよろしいですか？\n" },
            { ELanguage::German,   "Das Spiel muss neu gestartet werden um die\nfolgenden Änderungen zu speichern. OK?\n" },
            { ELanguage::French,   "Le jeu doit redémarrer pour appliquer\nles modifications suivantes. OK ?\n" },
            { ELanguage::Spanish,  "Se necesita reiniciar el juego para\naplicar los siguientes cambios. ¿OK?\n" },
            { ELanguage::Italian,  "Il gioco deve essere riavviato per\napplicare le seguenti modifiche. OK?\n" }
        }
    },
    {
        // PS5 confirms by closing the title; the player relaunches it manually.
        "Options_Message_RestartConfirmClose",
        {
            { ELanguage::English,  "Some changed settings take effect only after you\nclose and relaunch the game.\n\nClose the game now? Relaunch it afterward\nto apply these changes.\n" },
            { ELanguage::Japanese, "変更された設定はゲームを終了して\n再起動すると反映されます。\n\nゲームを終了しますか？\n変更を適用するには、あとで再起動してください。\n" },
            { ELanguage::German,   "Einige Änderungen werden erst wirksam, wenn\ndas Spiel geschlossen und neu gestartet wird.\n\nMöchtest du das Spiel jetzt schließen?\nStarte es danach neu, um die Änderungen anzuwenden.\n" },
            { ELanguage::French,   "Certains changements ne prennent effet qu’après\nla fermeture puis le redémarrage du jeu.\n\nVoulez-vous fermer le jeu maintenant ?\nRelancez-le ensuite pour appliquer ces changements.\n" },
            { ELanguage::Spanish,  "Algunos cambios solo se aplican al cerrar y\nvolver a iniciar el juego.\n\n¿Quieres cerrar el juego ahora?\nVuelve a iniciarlo para aplicar los cambios.\n" },
            { ELanguage::Italian,  "Alcune modifiche si applicano solo dopo aver\nchiuso e riavviato il gioco.\n\nVuoi chiudere il gioco ora?\nRiavvialo per applicare le modifiche.\n" }
        }
    },
    {
        "MainMenu_GoldMedalResults_Name",
        {
            { ELanguage::English,  "STATISTICS" },
            { ELanguage::Japanese, "STATISTICS" },
            { ELanguage::German,   "STATISTIKEN" },
            { ELanguage::French,   "STATISTIQUES" },
            { ELanguage::Spanish,  "ESTADÍSTICAS" },
            { ELanguage::Italian,  "STATISTICHE" }
        }
    },
    {
        "MainMenu_GoldMedalResults_Description",
        {
            { ELanguage::English,  "Statistics: Displays lists of Gold Medals and Achievements" },
            { ELanguage::Japanese, "スタティスティックス: ゴールドメダルと実績のリストを表示する" },
            { ELanguage::German,   "Statistiken: Zeigt eine Liste von Gold Medaillen und Erfolgen an" },
            { ELanguage::French,   "Statistiques : Affiche une liste des Médailles d'Or et des Accomplissements" },
            { ELanguage::Spanish,  "Estadísticas: Muestra los listados de las medallas y los logros" },
            { ELanguage::Italian,  "Statistiche: mostra gli elenchi delle medaglie d'oro e gli obiettivi" }
        }
    },
    {
        "Achievements_Title",
        {
            { ELanguage::English,  "Achievements" },
            { ELanguage::Japanese, "実績" },
            { ELanguage::German,   "Erfolge" },
            { ELanguage::French,   "Accomplissements" },
            { ELanguage::Spanish,  "Logros" },
            { ELanguage::Italian,  "Obiettivi" }
        }
    },
    {
        "Achievements_Title_Uppercase",
        {
            { ELanguage::English,  "ACHIEVEMENTS" },
            { ELanguage::Japanese, "実績" },
            { ELanguage::German,   "ERFOLGE" },
            { ELanguage::French,   "ACCOMPLISSEMENTS" },
            { ELanguage::Spanish,  "LOGROS" },
            { ELanguage::Italian,  "OBIETTIVI" }
        }
    },
    {
        "Achievements_GoldMedals",
        {
            { ELanguage::English,  "Gold Medals" },
            { ELanguage::Japanese, "ゴールドメダル" },
            { ELanguage::German,   "Gold Medaillen" },
            { ELanguage::French,   "Médailles d'or" },
            { ELanguage::Spanish,  "Medallas de oro" },
            { ELanguage::Italian,  "Medaglie d'oro" }
        }
    },
    {
        "Achievements_GoldMedals_Uppercase",
        {
            { ELanguage::English,  "GOLD MEDALS" },
            { ELanguage::Japanese, "ゴールドメダル" },
            { ELanguage::German,   "GOLD MEDAILLEN" },
            { ELanguage::French,   "MÉDAILLES D'OR" },
            { ELanguage::Spanish,  "MEDALLAS DE ORO" },
            { ELanguage::Italian,  "MEDAGLIE D'ORO" }
        }
    },
    {
        "Achievements_Unlock",
        {
            { ELanguage::English,  "Achievement Unlocked!" },
            { ELanguage::Japanese, "実績のロックが解除されました" },
            { ELanguage::German,   "Erfolg Freigeschaltet!" },
            { ELanguage::French,   "Succès déverrouillé !" },
            { ELanguage::Spanish,  "¡Logro desbloqueado!" },
            { ELanguage::Italian,  "Obiettivo sbloccato!" }
        }
    },
    {
        "Achievements_Progress",
        {
            { ELanguage::English,  "PROGRESS %d/%d" },
            { ELanguage::Japanese, "PROGRESS %d/%d" },
            { ELanguage::German,   "FORTSCHRITT %d/%d" },
            { ELanguage::French,   "PROGRESSION %d/%d" },
            { ELanguage::Spanish,  "PROGRESO %d/%d" },
            { ELanguage::Italian,  "PROGRESSI %d/%d" }
        }
    },
    {
        // Locale required for font atlas generation.
        "Installer_MusicCredits",
        {
            { ELanguage::English,  "♬ Result & Chill Lofi - Hotline Sehwani & SilverIceSound" },
        },
    },
    {
        "Installer_Header_Installer",
        {
            { ELanguage::English,  "INSTALLER" },
            { ELanguage::Japanese, "INSTALL" },
            { ELanguage::German,   "INSTALLATION" },
            { ELanguage::French,   "INSTALLATEUR" },
            { ELanguage::Spanish,  "INSTALADOR" },
            { ELanguage::Italian,  "INSTALLATORE" },
        },
    },
    {
        "Installer_Header_Installing",
        {
            { ELanguage::English,  "INSTALLING" },
            { ELanguage::Japanese, "INSTALL" },
            { ELanguage::German,   "INSTALLATION" },
            { ELanguage::French,   "INSTALLATION" },
            { ELanguage::Spanish,  "INSTALANDO" },
            { ELanguage::Italian,  "INSTALLANDO" },
        }
    },
    {
        "Installer_Page_SelectLanguage",
        {
            { ELanguage::English,  "Please select a language." },
            { ELanguage::Japanese, "言語を選択してください" },
            { ELanguage::German,   "Bitte eine Sprache auswählen." },
            { ELanguage::French,   "Choisissez une langue." },
            { ELanguage::Spanish,  "Selecciona un idioma." },
            { ELanguage::Italian,  "Seleziona una lingua." }
        }
    },
    {
        "Installer_Page_Introduction",
        {
            { ELanguage::English,  "Welcome to Marathon Recompiled!\n\nYou'll need an Xbox 360 copy of\nSONIC THE HEDGEHOG in order to proceed with the installation." },
            { ELanguage::Japanese, "Marathon Recompiledへようこそ！\n\nインストールにはXbox 360版の\n「ソニック・ザ・ヘッジホッグ」\nが必要です" },
            { ELanguage::German,   "Willkommen zu Marathon Recompiled!\n\nEs wird eine Xbox 360 Kopie von\nSONIC THE HEDGEHOG benötigt um mit der Installation fortfahren zu können." },
            { ELanguage::French,   "Bienvenue sur Marathon Recompiled !\n\nVous aurez besoin d'une copie de\nSONIC THE HEDGEHOG pour\nXbox 360 pour procéder à l'installation." },
            { ELanguage::Spanish,  "¡Bienvenido a Marathon Recompiled!\n\nNecesitas una copia de\nSONIC THE HEDGEHOG de\nXbox 360 para continuar con la instalación." },
            { ELanguage::Italian,  "Benvenuto a Marathon Recompiled!\n\nDovrai avere una copia di\nSONIC THE HEDGEHOG per la\nXbox 360 per proseguire con l'installazione." }
        }
    },
    {
        "Installer_Page_SelectGame",
        {
            { ELanguage::English,  "Add the sources for the game." },
            { ELanguage::Japanese, "ゲームのソースを追加" },
            { ELanguage::German,   "Füge die Quellen für das Spiel." },
            { ELanguage::French,   "Ajouter les fichiers du jeu." },
            { ELanguage::Spanish,  "Añade las fuentes para el juego." },
            { ELanguage::Italian,  "Aggiungi le fonti per il gioco." }
        }
    },
    {
        "Installer_Page_SelectDLC",
        {
            { ELanguage::English,  "Add the sources for the DLC." },
            { ELanguage::Japanese, "DLCのソースを追加" },
            { ELanguage::German,   "Füge die Quellen für die Erweiterungen des Spiels hinzu." },
            { ELanguage::French,   "Ajouter les fichiers pour les DLCs." },
            { ELanguage::Spanish,  "Añade las fuentes para el DLC." },
            { ELanguage::Italian,  "Aggiungi le fonti per i DLC." }
        }
    },
    {
        "Installer_Page_CheckSpace",
        {
            { ELanguage::English,  "The content will be installed to the program's folder.\n\n" },
            { ELanguage::Japanese, "コンテンツはプログラムのフォルダに\nインストールされます\n\n" },
            { ELanguage::German,   "Der Inhalt wird in dem Ordner des Programms installiert.\n\n" },
            { ELanguage::French,   "Le contenu sera installé dans le même dossier que le programme.\n\n" },
            { ELanguage::Spanish,  "El contenido será instalado a la carpeta del programa.\n\n" },
            { ELanguage::Italian,  "Il contenuto verrà installato nella cartella di questo programma.\n\n" }
        }
    },
    {
        "Installer_Page_Installing",
        {
            { ELanguage::English,  "Please wait while the content is being installed..." },
            { ELanguage::Japanese, "コンテンツのインストール中はお待ち\nください" },
            { ELanguage::German,   "Bitte warten. Der Inhalt wird installiert..." },
            { ELanguage::French,   "Veuillez patienter pendant l'installation du contenu..." },
            { ELanguage::Spanish,  "Por favor, espera mientras el contenido se instala... " },
            { ELanguage::Italian,  "Attendi mentre il contenuto viene installato... " }
        }
    },
    {
        "Installer_Page_InstallSucceeded",
        {
            { ELanguage::English,  "Installation complete!\n\nThis project is brought to you by:" },
            { ELanguage::Japanese, "インストール完了！\n\nプロジェクト制作：" },
            { ELanguage::German,   "Installation abgeschlossen!\n\nDieses Projekt wird präsentiert von:" },
            { ELanguage::French,   "Installation terminée !\n\nCe projet vous est présenté par :" },
            { ELanguage::Spanish,  "¡Instalación completada!\n\nEste proyecto ha sido posible gracias a:" },
            { ELanguage::Italian,  "Installazione completata!\n\nQuesto progetto è stato creato da:" }
        }
    },
    {
        "Installer_Page_InstallFailed",
        {
            { ELanguage::English,  "Installation failed.\n\n" },
            { ELanguage::Japanese, "インストールに失敗しました\n\n" },
            { ELanguage::German,   "Installation fehlgeschlagen.\n\n" },
            { ELanguage::French,   "L'installation a échoué.\n\n" },
            { ELanguage::Spanish,  "La instalación falló.\n\n" },
            { ELanguage::Italian,  "Installazione fallita.\n\n" }
        }
    },
    {
        "Installer_Step_Game",
        {
            { ELanguage::English,  "Game Data" },
            { ELanguage::Japanese, "ゲームデータ" },
            { ELanguage::German,   "Spieldaten" },
            { ELanguage::French,   "Fichiers du jeu" },
            { ELanguage::Spanish,  "Archivos del juego" },
            { ELanguage::Italian,  "Dati del gioco" }
        }
    },
    {
        "Installer_Step_RequiredSpace",
        {
            { ELanguage::English,  "Required space: %2.2f GiB" },
            { ELanguage::Japanese, "必要な容量: %2.2f GiB" },
            { ELanguage::German,   "Benötigter Speicherplatz:\n%2.2f GiB\n" },
            { ELanguage::French,   "Espace nécessaire : %2.2f Gio" },
            { ELanguage::Spanish,  "Espacio necesario: %2.2f GiB" },
            { ELanguage::Italian,  "Spazio necessario: %2.2f GiB" }
        }
    },
    {
        "Installer_Step_AvailableSpace",
        {
            { ELanguage::English,  "Available space: %2.2f GiB" },
            { ELanguage::Japanese, "使用可能な容量: %2.2f GiB" },
            { ELanguage::German,   "Verfügbarer Speicherplatz:\n%2.2f GiB\n" },
            { ELanguage::French,   "Espace disponible : %2.2f Gio" },
            { ELanguage::Spanish,  "Espacio disponible: %2.2f GiB" },
            { ELanguage::Italian,  "Spazio disponibile: %2.2f GiB" }
        }
    },
    {
        "Installer_Button_Next",
        {
            { ELanguage::English,  "Next" },
            { ELanguage::Japanese, "次へ" },
            { ELanguage::German,   "Weiter" },
            { ELanguage::French,   "Suivant" },
            { ELanguage::Spanish,  "Siguiente" },
            { ELanguage::Italian,  "Continua" }
        }
    },
    {
        "Installer_Button_Skip",
        {
            { ELanguage::English,  "Skip" },
            { ELanguage::Japanese, "スキップ" },
            { ELanguage::German,   "Überspringen" },
            { ELanguage::French,   "Ignorer" },
            { ELanguage::Spanish,  "Saltar" },
            { ELanguage::Italian,  "Salta" }
        }
    },
    {
        "Installer_Button_AddFiles",
        {
            { ELanguage::English,  "Add Files" },
            { ELanguage::Japanese, "ファイルを追加" },
            { ELanguage::German,   "Dateien hinzufügen" },
            { ELanguage::French,   "Ajouter des fichiers" },
            { ELanguage::Spanish,  "Añadir archivos" },
            { ELanguage::Italian,  "Aggiungi file" }
        }
    },
    {
        "Installer_Button_AddFolder",
        {
            { ELanguage::English,  "Add Folder" },
            { ELanguage::Japanese, "フォルダを追加" },
            { ELanguage::German,   "Ordner hinzufügen" },
            { ELanguage::French,   "Ajouter un dossier" },
            { ELanguage::Spanish,  "Añadir carpeta" },
            { ELanguage::Italian,  "Aggiungi cartella" }
        }
    },
    {
        // Notes: message appears when using the "Add Files" option and choosing any file that is not an Xbox 360 game dump.
        "Installer_Message_InvalidFilesList",
        {
            { ELanguage::English,  "The following selected files are invalid:" },
            { ELanguage::Japanese, "選択した次のファイルは無効です：" },
            { ELanguage::German,   "Die folgenden Dateien sind ungültig:" },
            { ELanguage::French,   "Les fichiers suivants ne sont pas valides :" },
            { ELanguage::Spanish,  "Los siguientes archivos no son válidos:" },
            { ELanguage::Italian,  "I seguenti file non sono validi:" }
        }
    },
    {
        // Notes: message appears in the event there are some invalid files after adding the DLC and moving onto the next step.
        "Installer_Message_InvalidFiles",
        {
            { ELanguage::English,  "Some of the files that have been provided are not valid. Please make sure all the specified files are correct and try again." },
            { ELanguage::Japanese, "提供されたファイルの一部が有効ではありません指定されたファイルがすべて正しいことを確認してもう一度お試しください" },
            { ELanguage::German,   "Einige Dateien, die bereitgestellt wurden sind ungültig. Bitte stelle sicher, dass die angegebenen Dateien korrekt sind und versuche es erneut." },
            { ELanguage::French,   "Certains fichiers fournis ne sont pas valides. Veuillez vous assurer que tous les fichiers spécifiés sont corrects et réessayez." },
            { ELanguage::Spanish,  "Algunos de los archivos seleccionados no son válidos. Por favor, asegúrate de que todos los archivos son correctos e inténtalo de nuevo." },
            { ELanguage::Italian,  "Alcuni dei file che sono stati selezionati non sono validi. Assicurati che tutti i file sono quelli corretti e riprova." }
        }
    },
    {
        // Notes: message appears when clicking the "Add Files" option for the first time.
        "Installer_Message_FilePickerTutorial",
        {
            { ELanguage::English,  "Select a digital dump with content from the game.\n\nThese files can be obtained from your Xbox 360 hard drive by following the instructions on the GitHub page.\n\nFor choosing a folder with extracted and unmodified game files, use the \"Add Folder\" option instead." },
            { ELanguage::Japanese, "ゲームのコンテンツを含む デジタルダンプを選択してください\n\nこれらのファイルは GitHubページの指示に従って\nXbox 360ハードドライブから取得できます\n\n抽出された変更されていないゲームファイルを含むフォルダーを選択するには代わりに「フォルダの追加」オプションを使用してください" },
            { ELanguage::German,   "Wähle einen digitalen Dump von dem Spiel.\n\nDie Dateien können über die Festplatte deiner\nXbox 360 erlangt werden. Folge hierfür den Anweisungen auf der GitHub Seite.\n\nUm einen Ordner mit unmodifizierten Spieldateien auszuwählen, benutze die \"Ordner hinzufügen\" Option stattdessen." },
            { ELanguage::French,   "Sélectionnez une copie dématérialisée avec le contenu du jeu de base.\n\nCes fichiers peuvent être obtenus à partir du disque dur de votre Xbox 360 en suivant les instructions de la page GitHub.\n\nPour choisir un dossier contenant les fichiers de jeu extraits et non modifiés, utilisez plutôt l'option \"Ajouter un dossier\"." },
            { ELanguage::Spanish,  "Selecciona una copia digital con contenido del juego.\n\nPuedes obtener los archivos de tu disco duro de\nXbox 360 siguiendo las instrucciones de la página de GitHub.\n\nPara elegir una carpeta con archivos extraídos sin modificar, utiliza la opción \"Añadir carpeta\"." },
            { ELanguage::Italian,  "Seleziona una copia digitale con i contenuti del gioco.\n\nQuesti file possono essere ottenuti dall'hard drive della tua Xbox 360 seguendo le istruzioni sulla pagina GitHub.\n\nPer selezionare una cartella con file estratti e non modificati usa l'opzione \"Aggiungi cartella\"." }
        }
    },
    {
        // Notes: message appears when clicking the "Add Folder" option for the first time.
        "Installer_Message_FolderPickerTutorial",
        {
            { ELanguage::English,  "Select a folder that contains the unmodified files that have been extracted from the game.\n\nThese files can be obtained from your Xbox 360 hard drive by following the instructions on the GitHub page.\n\nFor choosing a digital dump, use the \"Add Files\" option instead." },
            { ELanguage::Japanese, "ゲームから抽出された変更されていないファイルを含むフォルダを選択してください\n\nこれらのファイルは GitHubページの指示に従って\nXbox 360ハードドライブから取得できます\n\nデジタルダンプを選択するには\n代わりに「ファイルの追加」オプションを使用してください" },
            { ELanguage::German,   "Wähle einen Ordner, der unmodifizierte Dateien, die vom Spiel extrahiert wurden enthält.\n\nDie Dateien können über die Festplatte deiner\nXbox 360 erlangt werden. Folge hierfür den Anweisungen auf der GitHub Seite.\n\nUm einen digitalen Dump auszuwählen, benutze die \"Dateien hinzufügen\" Option stattdessen." },
            { ELanguage::French,   "Sélectionnez un dossier contenant les fichiers extraits du jeu de base.\n\nCes fichiers peuvent être obtenus à partir du disque dur de votre Xbox 360 en suivant les instructions de la page GitHub.\n\nPour choisir une copie dématérialisée, utilisez plutôt l'option \"Ajouter des fichiers\"." },
            { ELanguage::Spanish,  "Selecciona una carpeta que contenga los archivos sin modificar extraídos del juego.\n\nPuedes obtener los archivos de tu disco duro de\nXbox 360 siguiendo las instrucciones de la página de GitHub.\n\nPara elegir una copia digital, utiliza la opción \"Añadir archivos\"." },
            { ELanguage::Italian,  "Seleziona una cartella che contiene i file non modificati che sono stati estratti dal gioco.\n\nQuesti file possono essere ottenuti dall'hard drive della tua Xbox 360 seguendo le istruzioni sulla pagina GitHub.\n\nPer selezionare una copia digitale usa l'opzione \"Aggiungi file\"." }
        }
    },
    {
        // Notes: message appears when choosing the Install option at the title screen when the user is missing DLC content.
        // TODO: adjust line breaks for new message window.
        "Installer_Message_TitleMissingDLC",
        {
            { ELanguage::English,  "This will restart the game to\nallow you to install any DLC\nthat you are missing.\n\nWould you like to install missing\ncontent?" },
            { ELanguage::Japanese, "これによりゲームが再起動し不足しているDLCを\nインストールできるようになります\n\n不足しているコンテンツを\nインストールしますか？" },
            { ELanguage::German,   "Das Spiel wird neu gestartet\num die Installation einer fehlenden\nErweiterung zu ermöglichen.\n\nMöchtest du den fehlenden\nInhalt installieren?" },
            { ELanguage::French,   "Cela redémarrera le jeu pour vous\npermettre d'installer les DLC\nmanquants.\n\nSouhaitez-vous installer le\ncontenu manquant ?" },
            { ELanguage::Spanish,  "Esta opción reiniciará el juego\npara permitirte instalar los DLC\nque falten.\n\n¿Quieres instalar el contenido\nque falta?" },
            { ELanguage::Italian,  "Questa opzione riavviera il gioco\nper farti installare qualsiasi DLC\nche non hai installato.\n\nVuoi installare i DLC mancanti?" }
        }
    },
    {
        // Notes: message appears when choosing the Install option at the title screen when the user is not missing any content.
        // TODO: adjust line breaks for new message window.
        "Installer_Message_Title",
        {
            { ELanguage::English,  "This restarts the game to\nallow you to install any DLC\nthat you may be missing.\n\nYou are not currently\nmissing any DLC.\n\nWould you like to proceed\nanyway?" },
            { ELanguage::Japanese, "これによりゲームが再起動され\n不足しているDLCを\nインストールできるようになります\n\n現在 不足しているDLCはありません\n\nそれでも続行しますか？" },
            { ELanguage::German,   "Das Spiel wird neu gestartet\num die Installation einer fehlenden\nErweiterung zu ermöglichen.\n\nEs kann keine weitere Erweiterung\ninstalliert werden.\n\nMöchtest du trotzdem fortfahren?" },
            { ELanguage::French,   "Cela redémarrera le jeu pour vous\npermettre d'installer les DLC\nmanquants.\n\nIl ne vous manque aucun DLC.\n\nVoulez-vous quand même continuer ?" },
            { ELanguage::Spanish,  "Esto reiniciará el juego\npara permitirte instalar\nlos DLC que falten.\n\nActualmente, no falta ningún\nDLC por instalarse.\n\n¿Quieres continuar de todos\nmodos?" },
            { ELanguage::Italian,  "Questa opzione riavviera il gioco\nper farti installare qualsiasi DLC\nche non hai installato.\n\nHai già installato tutti i DLC.\n\nVuoi procedere comunque?" }
        }
    },
    {
        // Notes: message appears when user chooses "Quit" on the first available installation screen.
        "Installer_Message_Quit",
        {
            { ELanguage::English,  "Exit the installer.\nOK?" },
            { ELanguage::Japanese, "インストーラーを終了します\nよろしいですか？" },
            { ELanguage::German,   "Die Installation verlassen.\nOK?" },
            { ELanguage::French,   "Quitter l'installateur.\nOK ?" },
            { ELanguage::Spanish,  "¿Salir del instalador?" },
            { ELanguage::Italian,  "Esci dall'installatore.\nOK?" }
        }
    },
    {
        // Notes: message appears when user chooses "Cancel" during installation.
        "Installer_Message_Cancel",
        {
            { ELanguage::English,  "Cancel the installation.\nOK?" },
            { ELanguage::Japanese, "インストールをキャンセルします\nよろしいですか？" },
            { ELanguage::German,   "Die Installation abbrechen.\nOK?" },
            { ELanguage::French,   "Annuler l'installation.\nOK ?" },
            { ELanguage::Spanish,  "¿Cancelar la instalación?" },
            { ELanguage::Italian,  "Annulla l'installazione.\nOK?" }
        }
    },
    {
        // Notes: message appears when pressing B at the title screen.
        "Title_Message_Quit",
        {
            { ELanguage::English,  "Exit the game.\nOK?" },
            { ELanguage::Japanese, "ゲームを終了します\nよろしいですか？" },
            { ELanguage::German,   "Das Spiel verlassen.\nOK?" },
            { ELanguage::French,   "Quitter le jeu.\nOK ?" },
            { ELanguage::Spanish,  "¿Salir del juego?" },
            { ELanguage::Italian,  "Esci dal gioco.\nOK?" }
        }
    },
    {
        // Notes: message appears when SonicNextAchievementData.bin is corrupted (mismatching file size, bad signature, incorrect version or invalid checksum) upon loading save data.
        // To make this occur, open the file in any editor and just remove a large chunk of data.
        "Title_Message_LoadAchievementDataCorrupt",
        {
            { ELanguage::English,  "Load failed. Achievement data is corrupted.\nIf you continue your achievement progress will\nbe lost." },
            { ELanguage::Japanese, "ロード失敗。業績のデータが破損している。\nセーブしていない進行状況は失われます" },
            { ELanguage::German,   "Laden fehlgeschlagen. Erfolgs dateien sind\nkorrupt. Wenn du fortfährst wird dein Erfolgs\nfortschritt gelöscht." },
            { ELanguage::French,   "Chargement échoué. Les données d'Accomplissements\nsont corrompus. Si vous continuez, vos\naccomplissements seront perdus." },
            { ELanguage::Spanish,  "Error al cargar. Los datos de los logros están\ndañados. Si continúas, se perderá el progreso\nde tus logros." },
            { ELanguage::Italian,  "Caricamento fallito. I dati degli obiettivi\nsono danneggiati. Se continui perderai tutti\ni tuoi obiettivi." }
        }
    },
    {
        // Notes: message appears when SonicNextAchievementData.bin cannot be loaded upon loading save data.
        // To make this occur, lock the SonicNextAchievementData.bin file using an external program so that it cannot be accessed by the game.
        "Title_Message_LoadAchievementDataIOError",
        {
            { ELanguage::English,  "Load failed. Achievement data cannot be loaded.\nIf you continue you will not be able to save\nyour achievement progress." },
            { ELanguage::Japanese, "ロード失敗。業績のデータがロードできません。\n進行状況は失われますセーブしません" },
            { ELanguage::German,   "Laden fehlgeschlagen. Erfolgs dateien können nicht\ngeladen werden. Wenn du fortfährst wirst du deinen\nErfolgs fortschritt nicht speichern können." },
            { ELanguage::French,   "Chargement échoué. Les données d'Accomplissements\nn'ont pas pu être chargé. Si vous continuez, vous ne\npourrez pas sauvegarder vos accomplissements." },
            { ELanguage::Spanish,  "Error al cargar. No se pueden cargar los datos\nde los logros. Si continúas, no podrás guardar\nel progreso de tus logros." },
            { ELanguage::Italian,  "Caricamento fallito. Impossibile caricare i dati\ndegli obiettivi. Se continui non potrai salvare\ni tuoi obiettivi." }
        }
    },
    {
        // Notes: message appears when SonicNextAchievementData.bin cannot be saved upon saving save data.
        // To make this occur, lock the SonicNextAchievementData.bin file using an external program so that it cannot be accessed by the game.
        "Title_Message_SaveAchievementDataIOError",
        {
            { ELanguage::English,  "Save failed. Achievement data cannot be saved.\nIf you continue you will not be able to save\nyour achievement progress." },
            { ELanguage::Japanese, "セーブ失敗。業績のデータがセーブできません。\n進行状況は失われますセーブしません" },
            { ELanguage::German,   "Speichern fehlgeschlagen. Erfolgs dateien können\nnicht gespeichert werden. Wenn du fortfährst wirst\ndu deinen Erfolgs fortschritt nicht speichern können." },
            { ELanguage::French,   "Sauvegarde échoué. Les données d'Accomplissements\nn'ont pas pu être sauvegardé. Si vous continuez, vous ne\npourrez pas sauvegarder vos accomplissements." },
            { ELanguage::Spanish,  "Error al guardar. No se pueden guardar los datos\nde los logros. Si continúas, no podrás guardar\ntu progreso en los logros." },
            { ELanguage::Italian,  "Salvataggio fallito. Impossibile salvare i dati\ndegli obiettivi. Se continui non potrai salvare\ni tuoi obiettivi." }
        }
    },
    {
        "Title_Message_UpdateAvailable",
        {
            { ELanguage::English,  "An update is available!\n\nWould you like to visit the\nreleases page to download it?" },
            { ELanguage::Japanese, "アップデートが利用可能です\n\nリリースページにアクセスして\nダウンロードしますか？" },
            { ELanguage::German,   "Ein Update ist verfügbar!\n\nMöchtest du die Release-Seite\nbesuchen um es herunterzuladen?" },
            { ELanguage::French,   "Une mise à jour est disponible !\n\nVoulez-vous visiter la page\ndes mises à jour pour la\ntélécharger ?" },
            { ELanguage::Spanish,  "¡Hay una actualización disponible!\n\n¿Quieres ir a la página\npara descargarla?" },
            { ELanguage::Italian,  "È disponibile un aggiornamento!\n\nVuoi visitare la pagina releases\nper scaricarlo?" }
        }
    },
    {
        "Video_BackendError",
        {
            { ELanguage::English,  "Unable to create a D3D12 (Windows) or Vulkan backend.\n\nPlease make sure that:\n\n- Your system meets the minimum requirements.\n- Your GPU drivers are up to date.\n- Your operating system is on the latest version available." },
            { ELanguage::Japanese, "D3D12 (Windows)または\nVulkanバックエンドを作成できません\n\n次の点を確認してください：\n\n※システムが最小要件を満たしている\n※GPUドライバーが最新である\n※オペレーティングシステムが最新バージョンである" },
            { ELanguage::German,   "Es ist nicht möglich, ein D3D12 (Windows) oder Vulkan-Backend zu erstellen.\n\nBitte stelle sicher, dass:\n\n- Dein System die Mindestanforderungen erfüllt.\n- Deine GPU-Treiber auf dem neuesten Stand sind.\n- Dein Betriebssystem auf der neuesten verfügbaren Version ist." },
            { ELanguage::French,   "Impossible de créer un backend D3D12 (Windows) ou Vulkan.\n\nVeuillez vous assurer que :\n\n- Votre système répond aux critères minimums requis.\n- Les pilotes de votre processeur graphique sont à jour.\n- Votre système d'exploitation est à jour." },
            { ELanguage::Spanish,  "No se puede crear un entorno de D3D12 (Windows) o de Vulkan.\n\nPor favor, asegúrate de que:\n\n- Tu equipo cumple con los requisitos mínimos.\n- Los drivers de tu tarjeta gráfica están actualizados.\n- Tu sistema operativo está actualizado a la última versión.\n" },
            { ELanguage::Italian,  "Impossibile creare un backend D3D12 (Windows) o Vulkan.\n\nAssicurati che:\n\n- Il tuo sistema soddisfi i requisiti minimi.\n- I driver della scheda grafica siano aggiornati.\n- Il tuo sistema operativo sia aggiornato." }
        }
    },
    {
        "System_Win32_MissingDLLs",
        {
            { ELanguage::English,  "The module \"%s\" could not be found.\n\nPlease make sure that:\n\n- You extracted this copy of Marathon Recompiled fully and not just the *.exe file.\n- You are not running Marathon Recompiled from a *.zip file." },
            { ELanguage::Japanese, "モジュール\"%s\"が見つかりませんでした\n\n次の点を確認してください：\n\n※Marathon Recompiledの*.exeファイルだけを抽出していなく、 コピーを完全に抽出してること\n※Marathon Recompiledを*.zipファイルから実行していないこと" },
            { ELanguage::German,   "Das Modul \"%s\" konnte nicht gefunden werden.\n\nBitte stelle sicher, dass:\n\n- Diese Kopie von Marathon Recompiled vollständig entpackt wurde und nicht nur die *.exe-Datei.\n- Marathon Recompiled nicht direkt aus einer *.zip-Datei ausgeführt wird." },
            { ELanguage::French,   "Le module \"%s\" n'a pas pu être trouvé.\n\nVeuillez vous assurer que :\n\n- Vous avez extrait Marathon Recompiled dans son entièreté et pas seulement le fichier *.exe.\n- Vous n'exécutez pas Marathon Recompiled à partir d'un fichier *.zip." },
            { ELanguage::Spanish,  "No se pudo encontrar el módulo \"%s\".\n\nAsegúrese de que:\n\n- Ha extraido esta copia de Marathon Recompiled por completo y no solo el archivo *.exe.\n- No está ejecutando Marathon Recompiled desde un archivo *.zip." },
            { ELanguage::Italian,  "Impossibile trovare il modulo \"%s\".\n\nAssicurati che:\n\n- Hai estratto questa copia di Marathon Recompiled correttamente e non solo il file *.exe.\n- Non stai eseguendo Marathon Recompiled da un file *.zip." }
        }
    },
    {
        "System_MemoryAllocationFailed",
        {
            { ELanguage::English,  "Failed to allocate game memory.\n\nPlease make sure that:\n\n- You meet the minimum system requirements (8 GB).\n- Your page file is configured with at least 4-8 GB of virtual memory." },
            { ELanguage::Japanese, "ゲームメモリの割り当てに失敗しました\n\n次の点を確認してください：\n\n※最小システム要件（8 GB）を満たしていること\n※ページファイルに少なくとも4～8 GBの仮想メモリが設定されていること" },
            { ELanguage::German,   "Fehler beim Zuweisen des Spielspeichers.\n\nBitte stelle sicher, dass:\n\n- Die Mindestanforderungen für das System erfüllt sind (8 GB).\n- Die Auslagerungsdatei mit mindestens 4-8 GB virtuellem Speicher konfiguriert ist." },
            { ELanguage::French,   "Échec d'allocation de la mémoire du jeu.\n\nVeuillez vous assurer que :\n\n- Vous disposez de la configuration minimale requise (8 Go).\n- Votre fichier d'échange est configuré avec au moins 4 à 8 Go de mémoire virtuelle." },
            { ELanguage::Spanish,  "Fallo al asignar memoria del juego.\n\nPor favor, asegúrate de que:\n\n- Cumples los requisitos mínimos del sistema (8 GB).\n- Tu archivo de páginación está configurado con al menos 4 u 8 GB de memoria virtual." },
            { ELanguage::Italian,  "Impossibile allocare la memoria per il gioco.\n\nAssicurati che:\n\n- Soddisfi i requisiti minimi di sistema (8 GB).\n- Il tuo file di paging sia configurato con almeno 4 o 8 GB di memoria virtuale." }
        }
    },
    {
        "IntegrityCheck_Success",
        {
            { ELanguage::English,  "Installation check has finished.\n\nAll files seem to be correct.\n\nThe game will now close. Remove the launch argument to play the game." },
            { ELanguage::Japanese, "インストールチェックが終了しました\n\nすべてのファイルは正しいようです\n\nゲームは終了します、ゲームをプレイするには起動引数を削除してください" },
            { ELanguage::German,   "Die Installation wurde überprüft.\n\nAlle Dateien scheinen korrekt zu sein.\n\nDas Spiel wird nun geschlossen. Entferne die Startoption, um das Spiel zu spielen." },
            { ELanguage::French,   "La vérification de l'installation est terminée.\n\nTous les fichiers semblent corrects.\n\nL'application va maintenant se fermer. Retirez l'argument de lancement pour pouvoir lancer le jeu." },
            { ELanguage::Spanish,  "La verificación de la instalación ha terminado.\n\nTodos los archivos parecen correctos.\n\nEl juego se cerrará ahora. Elimina el argumento de lanzamiento para jugar al juego." },
            { ELanguage::Italian,  "La verifica dei file d'installazione è terminata.\n\nTutti i file sembrano corretti.\n\nIl gioco si chiuderà. Rimuovi l'argomento di avvio per poter giocare." }
        }
    },
    {
        "IntegrityCheck_Failed",
        {
            { ELanguage::English,  "Installation check has failed.\n\nError: %s\n\nThe game will now close. Try reinstalling the game by using the --install launch argument." },
            { ELanguage::Japanese, "インストールチェックに失敗しました\n\nエラー：%s\n\nゲームは終了します、--install 起動引数を使用してゲームを再インストールしてください" },
            { ELanguage::German,   "Die Installationsprüfung ist fehlgeschlagen.\n\nFehler: %s\n\nDas Spiel wird nun geschlossen. Versuche das Spiel durch Verwendung der Startoption --install neu zu installieren." },
            { ELanguage::French,   "La vérification de l'installation a échoué.\n\nErreur : %s\n\nL'application va maintenant se fermer. Essayez de réinstaller le jeu en utilisant l'argument de lancement --install." },
            { ELanguage::Spanish,  "La verificación de la instalación ha fallado.\n\nError: %s\n\nEl juego se cerrará ahora. Intenta reinstalar el juego utilizando el argumento de lanzamiento --install." },
            { ELanguage::Italian,  "La verifica dei file d'installazione non è andata a buon fine.\n\nErrore: %s\n\nIl gioco si chiuderà. Prova a reinstallare il gioco utilizzando l'argomento di avvio --install." }
        }
    },
    {
        "Common_OK",
        {
            { ELanguage::English,  "OK" },
            { ELanguage::Japanese, "OK" },
            { ELanguage::German,   "OK" },
            { ELanguage::French,   "OK" },
            { ELanguage::Spanish,  "OK" },
            { ELanguage::Italian,  "OK" }
        }
    },
    {
        "Common_On",
        {
            { ELanguage::English,  "On" },
            { ELanguage::Japanese, "オン" },
            { ELanguage::German,   "An" },
            { ELanguage::French,   "Oui" },
            { ELanguage::Spanish,  "Act." },
            { ELanguage::Italian,  "Sì" }
        }
    },
    {
        "Common_Off",
        {
            { ELanguage::English,  "Off" },
            { ELanguage::Japanese, "オフ" },
            { ELanguage::German,   "Aus" },
            { ELanguage::French,   "Non" },
            { ELanguage::Spanish,  "Desact." },
            { ELanguage::Italian,  "No" }
        }
    },
    {
        "Common_Yes",
        {
            { ELanguage::English,  "Yes" },
            { ELanguage::Japanese, "はい" },
            { ELanguage::German,   "Ja" },
            { ELanguage::French,   "Oui" },
            { ELanguage::Spanish,  "Sí" },
            { ELanguage::Italian,  "Sì" }
        }
    },
    {
        "Common_No",
        {
            { ELanguage::English,  "No" },
            { ELanguage::Japanese, "いいえ" },
            { ELanguage::German,   "Nein" },
            { ELanguage::French,   "Non" },
            { ELanguage::Spanish,  "No" },
            { ELanguage::Italian,  "No" }
        }
    },
    {
        "Common_Next",
        {
            { ELanguage::English,  "Next" },
            { ELanguage::Japanese, "次へ" },
            { ELanguage::German,   "Weiter" },
            { ELanguage::French,   "Suivant" },
            { ELanguage::Spanish,  "Siguiente" },
            { ELanguage::Italian,  "Avanti" }
        }
    },
    {
        "Common_Select",
        {
            { ELanguage::English,  "Select" },
            { ELanguage::Japanese, "決定" },
            { ELanguage::German,   "Auswählen" },
            { ELanguage::French,   "Sélectionner" },
            { ELanguage::Spanish,  "Seleccionar" },
            { ELanguage::Italian,  "Seleziona" }
        }
    },
    {
        "Common_Back",
        {
            { ELanguage::English,  "Back" },
            { ELanguage::Japanese, "戻る" },
            { ELanguage::German,   "Zurück" },
            { ELanguage::French,   "Retour" },
            { ELanguage::Spanish,  "Atrás" },
            { ELanguage::Italian,  "Indietro" }
        }
    },
    {
        "Common_Quit",
        {
            { ELanguage::English,  "Quit" },
            { ELanguage::Japanese, "やめる" },
            { ELanguage::German,   "Abbrechen" },
            { ELanguage::French,   "Annuler" },
            { ELanguage::Spanish,  "Cancelar" },
            { ELanguage::Italian,  "Annulla" }
        }
    },
    {
        "Common_Cancel",
        {
            { ELanguage::English,  "Cancel" },
            { ELanguage::Japanese, "キャンセル" },
            { ELanguage::German,   "Abbrechen" },
            { ELanguage::French,   "Annuler" },
            { ELanguage::Spanish,  "Cancelar" },
            { ELanguage::Italian,  "Annulla" }
        }
    },
    {
        "Common_Reset",
        {
            { ELanguage::English,  "Reset" },
            { ELanguage::Japanese, "リセット" },
            { ELanguage::German,   "Zurücksetzen" },
            { ELanguage::French,   "Par défaut" },
            { ELanguage::Spanish,  "Restablecer" },
            { ELanguage::Italian,  "Ripristina" }
        }
    },
    {
        "Common_Switch",
        {
            { ELanguage::English,  "Switch" },
            { ELanguage::Japanese, "きりかえ" },
            { ELanguage::German,   "Wechseln" },
            { ELanguage::French,   "Changer" },
            { ELanguage::Spanish,  "Cambiar" },
            { ELanguage::Italian,  "Cambia" }
        }
    },
    {
        "Common_Retry",
        {
            { ELanguage::English,  "Retry" },
            { ELanguage::Japanese, "リトライ" },
            { ELanguage::German,   "Wiederholen" },
            { ELanguage::French,   "Réessayer" },
            { ELanguage::Spanish,  "Reintentar" },
            { ELanguage::Italian,  "Riprova" }
        }
    },
    {
        "Common_ContinueWithoutSaving",
        {
            { ELanguage::English,  "Continue without saving." },
            { ELanguage::Japanese, "セーブせずにゲームを続けます" },
            { ELanguage::German,   "Fortsetzen ohne zu speichern." },
            { ELanguage::French,   "Continuer sans sauvegarder." },
            { ELanguage::Spanish,  "Continuar sin guardar." },
            { ELanguage::Italian,  "Continua senza salvare." }
        }
    },
    {
        "Button_Cancel",
        {
            { ELanguage::English, "${picture(button_b)}${locale(Common_Cancel)}" },
            { ELanguage::Spanish, "${picture(button_b)} ${locale(Common_Cancel)}" }
        }
    },
    {
        "Button_Back",
        {
            { ELanguage::English, "${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish, "${picture(button_b)} ${locale(Common_Back)}" }
        }
    },
    {
        "Button_Select",
        {
            { ELanguage::English, "${picture(button_a)}${locale(Common_Select)}" },
            { ELanguage::Spanish, "${picture(button_a)} ${locale(Common_Select)}" },
        }
    },
    {
        "Button_SelectQuit",
        {
            { ELanguage::English, "${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Quit)}" },
            { ELanguage::German,  "${picture(button_a)}${locale(Common_Select)}　　${picture(button_b)}${locale(Common_Quit)}" },
            { ELanguage::Spanish, "${picture(button_a)} ${locale(Common_Select)}  ${picture(button_b)} ${locale(Common_Quit)}" },
        }
    },
    {
        "Button_SelectQuitMods",
        {
            { ELanguage::English,  "${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Quit)}  ${picture(button_x)}${locale(Mods_Button_Open)}" },
            { ELanguage::Japanese, "${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Quit)}  ${picture(button_x)}${locale(Mods_Button_Open)}" },
            { ELanguage::German,   "${picture(button_a)}${locale(Common_Select)}　　${picture(button_b)}${locale(Common_Quit)}　　${picture(button_x)}${locale(Mods_Button_Open)}" },
            { ELanguage::French,   "${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Quit)}  ${picture(button_x)}${locale(Mods_Button_Open)}" },
            { ELanguage::Spanish,  "${picture(button_a)} ${locale(Common_Select)}  ${picture(button_b)} ${locale(Common_Quit)}  ${picture(button_x)} ${locale(Mods_Button_Open)}" },
            { ELanguage::Italian,  "${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Quit)}  ${picture(button_x)}${locale(Mods_Button_Open)}" }
        }
    },
    {
        "Mods_DetectedStatus",
        {
            { ELanguage::English,  "Mods detected: {}." },
            { ELanguage::Japanese, "{}件のMODを検出しました。" },
            { ELanguage::German,   "{} Mods erkannt." },
            { ELanguage::French,   "{} mods détectés." },
            { ELanguage::Spanish,  "Se detectaron {} mods." },
            { ELanguage::Italian,  "Rilevati {} mod." }
        }
    },
    {
        "Button_Mods_ToggleOptionsBack",
        {
            { ELanguage::English,  "${picture(button_a)}${locale(Mods_Button_Toggle)}  ${picture(button_x)}${locale(Mods_Button_Options)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Japanese, "${picture(button_a)}${locale(Mods_Button_Toggle)}  ${picture(button_x)}${locale(Mods_Button_Options)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,   "${picture(button_a)}${locale(Mods_Button_Toggle)}　　${picture(button_x)}${locale(Mods_Button_Options)}　　${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::French,   "${picture(button_a)}${locale(Mods_Button_Toggle)}  ${picture(button_x)}${locale(Mods_Button_Options)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish,  "${picture(button_a)} ${locale(Mods_Button_Toggle)}  ${picture(button_x)} ${locale(Mods_Button_Options)}  ${picture(button_b)} ${locale(Common_Back)}" },
            { ELanguage::Italian,  "${picture(button_a)}${locale(Mods_Button_Toggle)}  ${picture(button_x)}${locale(Mods_Button_Options)}  ${picture(button_b)}${locale(Common_Back)}" }
        }
    },
    {
        "Button_Mods_ChangeBack",
        {
            { ELanguage::English,  "${picture(button_x)}${locale(Mods_Button_Change)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Japanese, "${picture(button_x)}${locale(Mods_Button_Change)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,   "${picture(button_x)}${locale(Mods_Button_Change)}　　${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::French,   "${picture(button_x)}${locale(Mods_Button_Change)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish,  "${picture(button_x)} ${locale(Mods_Button_Change)}  ${picture(button_b)} ${locale(Common_Back)}" },
            { ELanguage::Italian,  "${picture(button_x)}${locale(Mods_Button_Change)}  ${picture(button_b)}${locale(Common_Back)}" }
        }
    },
    {
        "Button_Mods_EmptyBack",
        {
            { ELanguage::English,  "${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Japanese, "${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,   "${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::French,   "${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish,  "${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Italian,  "${picture(button_b)}${locale(Common_Back)}" }
        }
    },
    {
        "Button_SelectBack",
        {
            { ELanguage::English, "${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,  "${picture(button_a)}${locale(Common_Select)}　　${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish, "${picture(button_a)} ${locale(Common_Select)}  ${picture(button_b)} ${locale(Common_Back)}" },
        }
    },
    {
        "Button_ResetSelectBack",
        {
            { ELanguage::English, "${picture(button_x)}${locale(Common_Reset)}  ${picture(button_a)}${locale(Common_Select)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,  "${picture(button_x)}${locale(Common_Reset)}　　${picture(button_a)}${locale(Common_Select)}　　${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish, "${picture(button_x)} ${locale(Common_Reset)}  ${picture(button_a)} ${locale(Common_Select)}  ${picture(button_b)} ${locale(Common_Back)}" },
        }
    },
    {
        "Button_GoldMedalsBack",
        {
            { ELanguage::English, "${picture(button_y)}${locale(Achievements_GoldMedals)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,  "${picture(button_y)}${locale(Achievements_GoldMedals)}　　${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish, "${picture(button_y)} ${locale(Achievements_GoldMedals)}  ${picture(button_b)} ${locale(Common_Back)}" }
        }
    },
    {
        "Button_AchievementsBack",
        {
            { ELanguage::English, "${picture(button_y)}${locale(Achievements_Title)}  ${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::German,  "${picture(button_y)}${locale(Achievements_Title)}　　${picture(button_b)}${locale(Common_Back)}" },
            { ELanguage::Spanish, "${picture(button_y)} ${locale(Achievements_Title)}  ${picture(button_b)} ${locale(Common_Back)}" }
        }
    }
};

std::string& Localise(const std::string_view& key)
{
    auto localeFindResult = g_locale.find(key);

    if (localeFindResult != g_locale.end())
    {
        auto languageFindResult = localeFindResult->second.find(Config::Language);

        if (languageFindResult == localeFindResult->second.end())
            languageFindResult = localeFindResult->second.find(ELanguage::English);

        if (languageFindResult != localeFindResult->second.end())
            return languageFindResult->second;
    }

    return g_localeMissing;
}
