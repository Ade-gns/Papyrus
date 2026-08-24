# Papyrus — État d'avancement

Suite bureautique PDF pour Linux, en C++/Qt6. Ce document récapitule ce qui est fait, comment c'est validé, et ce qu'il reste à faire. Mis à jour au fil des sessions.

## Architecture

- **Stack** : C++20 / Qt6 (Widgets, Pdf, PdfWidgets, PrintSupport, Concurrent) + PDFium (API C directe, vendorisée) + LibreOffice headless (subprocess) + Tesseract OCR (subprocess).
- **Modules** (`libs/`) :
  - `pdf-engine` — lecture (`Document`, wrapper `QPdfDocument`), miniatures (`ThumbnailCache`), édition de pages (`PageEditor`, PDFium brut), annotations/formes/images (`AnnotationWriter`), formulaires (`FormEngine`).
  - `conversion-engine` — TXT→PDF et images→PDF (Qt natif : `QTextDocument`/`QPrinter`, `QPdfWriter`), conversion Office (`OfficeToPdfJob`, LibreOffice headless).
  - `ocr-engine` — `OcrJob` (Tesseract + fusion des pages via `PageEditor`).
- **App** (`app/`) : `MainWindow`, onglets (`DocumentTab`), et une douzaine de dialogues (gestion de pages, annotation, signature, conversion, OCR, formulaires...).
- PDFium n'est **pas commité** : `scripts/fetch-pdfium.sh` le télécharge dans `third_party/pdfium/` (ignoré par git), avec vérification de somme de contrôle.

## Phases terminées

| Phase | Contenu | Statut |
|---|---|---|
| 1 | Analyse, choix de la stack (C++/Qt6 retenu après comparaison), architecture | ✅ |
| 2 | MVP : ouverture, affichage, zoom, navigation, miniatures, onglets, glisser-déposer | ✅ testé réel |
| 3 | Recherche (Ctrl+F), gestion des pages (rotation/suppression/duplication/réorganisation/extraction/fusion), annotations (surlignage + rectangle/cercle) | ✅ testé réel |
| 4 | Création PDF depuis texte (mise en page complète) et images (une par page) | ✅ testé réel |
| 5 | Conversion Office (DOC/DOCX/ODT, PPT/PPTX/ODP) via LibreOffice headless | ✅ testé réel |
| 6 | Signature : dessin souris, import + suppression fond blanc, saisie clavier (6 polices manuscrites OFL), placement avec rotation, intégrée comme vrai contenu de page | ✅ testé réel |
| 7 | OCR (Tesseract, PDF recherchable) + remplissage de formulaires (texte/case à cocher/liste déroulante) via l'API interactive PDFium | ✅ testé réel |
| 8 | Impression (CUPS), Undo/Redo + écriture atomique + récupération après crash, optimisation des performances (vignettes à la demande) — détail ci-dessous | ✅ testé réel |

Chaque phase a été validée par des tests directs sur de vrais fichiers (jamais de suppositions non vérifiées) — voir la section "Pièges trouvés" ci-dessous pour les bugs réels découverts en cours de route.

## Limitations connues et décisions assumées

- **`QPdfView` (visualiseur principal) n'affiche pas les annotations.** Qt n'expose aucune option publique pour ça. Décision (validée avec l'utilisateur) : accepter la limitation pour l'instant plutôt que de remplacer tout le moteur de rendu. Les annotations sont bien enregistrées dans le fichier et visibles dans tout autre lecteur PDF.
  - Contournement pour les **signatures/images** : insérées comme vrai contenu de page (pas une annotation) → toujours visibles dans notre propre visualiseur.
- **Annotations non supportées** : dessin libre (encre), lignes/flèches, zones de texte libre. Testées, PDFium les enregistre mais ne les affiche pas sans flux d'apparence construit à la main. Seuls Highlight, Rectangle et Circle sont fiables.
- **Formulaires** : champs texte, cases à cocher, boutons radio, listes déroulantes/à choix supportés. Pas de création de nouveaux champs (hors périmètre initial). Formulaires XFA non supportés.
- **Pas d'en-tête/pied de page personnalisés** pour la création TXT→PDF (Qt ne le permet pas nativement sans peindre chaque page à la main).
- **Une seule image par page** pour la création images→PDF (pas de mise en page multi-images sur une page).
- OCR : le pack de langue française (`tesseract-ocr-fra`) doit être installé séparément si absent — détecté dynamiquement, pas de dépendance dure.

## Pièges techniques trouvés (à ne pas refaire)

- `slots` est une macro réservée par Qt (signaux/slots des `QObject`) — un paramètre ou une variable nommée `slots` se fait silencieusement effacer par le préprocesseur.
- AUTOMOC de CMake ne détecte pas les en-têtes `Q_OBJECT` situés dans un dossier différent des sources (`include/` vs `src/`) — il faut les lister explicitement dans `add_library`/`add_executable`.
- Les annotations Square/Circle de PDFium ont besoin d'une couleur de **remplissage** (`FPDFANNOT_COLORTYPE_InteriorColor`), pas seulement de bordure, sinon rien ne s'affiche malgré une sauvegarde « réussie ».
- Les annotations à quadpoints (Highlight...) ont aussi besoin que `/Rect` soit défini explicitement, en plus des quadpoints.
- `QFontDatabase` plante (segfault) si utilisé avec un `QCoreApplication` au lieu d'un `QGuiApplication`/`QApplication`.
- PDFium n'a **aucune fonction directe** pour fixer la valeur d'un champ de formulaire — il faut simuler une vraie interaction (`FORM_SetFocusedAnnot` + `FORM_SelectAllText` + `FORM_ReplaceSelection` pour le texte, clic simulé pour les cases à cocher, `FORM_SetIndexSelected` pour les listes — qui échoue silencieusement sans focus préalable).

## Phase 8 — détail

| Volet | Contenu | Statut |
|---|---|---|
| Impression | Rendu page-à-page (`Document::renderPage`) vers `QPrinter`, mise à l'échelle + centrage sur la zone imprimable, respect de la plage de pages / page courante choisie dans le dialogue. `Fichier > Imprimer...` (Ctrl+P, `QPrintDialog`) et `Fichier > Aperçu avant impression...` (`QPrintPreviewDialog`), backend CUPS détecté sur la machine de dev. | ✅ testé réel (rendu vérifié en générant un PDF via le chemin d'impression et en le rouvrant dans l'app) |
| Undo/Redo + autosauvegarde + récupération crash | Historique de versions par fichier (`DocumentHistory`, pile undo/redo persistée sur disque), câblé sur les 4 dialogues qui éditent un document en place (pages, annotations, signature, formulaires) via `Édition > Annuler/Rétablir` (Ctrl+Z / Ctrl+Maj+Z). Écriture atomique (`QSaveFile`) partagée par tous les writers PDFium — un crash pendant une sauvegarde ne corrompt plus le fichier original. Récupération de session : les onglets ouverts sont suivis en continu ; si le dernier arrêt n'était pas propre, l'app propose de les rouvrir au démarrage. | ✅ testé réel (voir notes ci-dessous) |
| Optimisation performances | Chargement des vignettes à la demande (zone visible + marge de préchargement) au lieu de rendre toutes les pages dès l'ouverture d'un document ; cache de vignettes plafonné (300 entrées, éviction FIFO). | ✅ testé réel (voir notes ci-dessous) |

- **Phase 9** : packaging — `.deb`, `.rpm`, AppImage, Flatpak, script d'installation en une commande, système de mise à jour.

## Notes techniques Phase 8 — impression

- Nouveau : `app/src/document_printer.h/.cpp` — fonction libre `papyrus::printDocument(Document&, QPrinter&, currentPageIndex)`, sans dépendance Widgets côté `pdf-engine` (reste dans `app/`, qui lie déjà `Qt6::PrintSupport`).
- Échelle calculée en comparant la taille de page en points (`Document::pagePointSize`) convertie en pixels à la résolution de l'imprimante (`printer.resolution()`), contre `printer.pageRect(QPrinter::DevicePixel)` — mêmes unités des deux côtés, sinon le calcul d'échelle est faux.
- `QPrintDialog` : options `PrintPageRange` + `PrintCurrentPage` activées ; `printDocument` lit `printer.printRange()` pour restreindre les pages rendues.

## Notes techniques Phase 8 — Undo/Redo, écriture atomique, récupération crash

- **Décision d'architecture (validée avec l'utilisateur)** : l'app n'a pas de session d'édition en mémoire — chaque dialogue (pages/annotations/signature/formulaires) sauvegarde directement sur disque dès validation. Undo/Redo est donc **au niveau fichier** (instantanés du document entier), pas une pile de commandes granulaires en mémoire. Plus simple, s'intègre sans refonte à l'architecture existante.
- **Bug de corruption trouvé en chemin (corrigé)** : les trois writers PDFium (`PageEditor`, `AnnotationWriter`, `FormEngine`) ouvraient le fichier cible directement en `Truncate` avant d'écrire — un crash pendant `FPDF_SaveAsCopy` détruisait le document original sans qu'aucune donnée valide ne subsiste. Remplacé par `detail::AtomicPdfWriter` (partagé, `libs/pdf-engine/src/pdfium_runtime.h/.cpp`), basé sur `QSaveFile` : écrit dans un fichier temporaire, ne remplace l'original qu'au `commit()` réussi. Testé : sauvegarde in-place (même chemin source/destination, cas réel de l'app) fonctionne correctement, y compris avec PDFium ayant le document source chargé au moment du remplacement.
- **`DocumentHistory`** (`libs/pdf-engine/include/papyrus/pdf/document_history.h` + `.cpp`) : pile undo/redo par fichier (clé = SHA1 du chemin canonique), persistée sous `QStandardPaths::AppDataLocation/history/<hash>/{undo,redo}/`, plafonnée à 20 niveaux. `capture(path)` lit les octets courants avant qu'un dialogue ne sauvegarde ; `commit(path, before)` empile ce `before` comme point d'annulation et vide la pile redo (nouvelle branche). Persisté sur disque (pas en mémoire) → survit à un redémarrage de l'app, donc Ctrl+Z fonctionne encore après un crash.
  - Piège trouvé en testant (et corrigé) : nommer les instantanés par timestamp milliseconde crée des collisions silencieuses lors d'écritures rapprochées (deux `commit()` dans la même milliseconde s'écrasent). Remplacé par un compteur strictement croissant (`max des noms existants + 1`), robuste même après élagage des plus anciennes entrées par le plafond.
  - Testé réel (harnais headless hors GUI, car pas d'outil d'automatisation d'entrée clavier/souris disponible dans cet environnement) : undo/redo multi-niveaux, invalidation de la pile redo après une nouvelle édition suivant un undo, plafond à 20 niveaux avec élagage par le bas sans collision de nom.
- **Câblage UI** (`MainWindow`) : menu `&Édition` avec `Annuler`/`Rétablir` (`QKeySequence::Undo/Redo`), activés/désactivés via `DocumentHistory::canUndo/canRedo` sur le fichier de l'onglet courant. `openPageManager`/`openAnnotationDialog`/`openSignatureDialog`/`openFormFillDialog` appellent `DocumentHistory::capture()` juste avant `dialog->exec()`, puis `commit()` dans le lambda connecté au signal `documentSaved` du dialogue (donc uniquement si la sauvegarde a vraiment eu lieu, pas en cas d'annulation).
- **Récupération après fermeture inattendue** : liste des onglets ouverts suivie en continu dans `QSettings` (`openTabs`, ajout/retrait à l'ouverture/fermeture d'onglet) + drapeau `cleanShutdown` mis à `false` au démarrage et à `true` dans `MainWindow::closeEvent`. Si `cleanShutdown` était resté à `false` au lancement suivant (arrêt brutal), propose de rouvrir les documents encore listés comme ouverts. Testé réel : dialogue de récupération confirmé à l'écran en simulant un arrêt brutal (édition manuelle de `~/.config/Papyrus/Papyrus.conf` + relance), et confirmé absent sur un lancement sans état à récupérer.
  - Limite connue : le clic sur « Oui »/« Non » du dialogue de récupération n'a pas pu être testé interactivement (pas d'outil d'automatisation clavier/souris dans cet environnement) — la logique de réouverture réutilise `openFile()`, déjà exercée ailleurs.
  - Confirmé en conditions réelles (pas seulement simulé) : un `kill -9` accidentel de l'app pendant les tests précédents a bien laissé `cleanShutdown=false` + l'onglet ouvert dans `openTabs`, et le dialogue de récupération est réapparu tout seul au lancement suivant.

## Notes techniques Phase 8 — optimisation des performances

- **Problème trouvé** : `ThumbnailPanel::setDocumentTab` demandait le rendu de **toutes** les pages du document dès l'ouverture d'un onglet (boucle sur `pageCount()`), en appelant `ThumbnailCache::thumbnail()` pour chacune — le code le signalait déjà lui-même en commentaire comme raccourci MVP. Sur un document de plusieurs centaines de pages, ça mettait en file des centaines de rendus sur l'unique thread de fond du cache, pour des pages que l'utilisateur ne voit peut-être jamais. Le cache (`ThumbnailCache::m_cache`) était aussi non borné (aucune éviction).
- **Corrigé** :
  - `ThumbnailPanel::requestVisibleThumbnails()` calcule la plage de lignes réellement visible dans le panneau (`QListWidget::indexAt` sur les coins du viewport, avec repli sur une estimation par hauteur de ligne si `indexAt` échoue) plus une marge de préchargement de 8 pages de chaque côté, et ne demande le rendu que pour cette plage. Déclenché à l'ouverture d'un document (différé via `QTimer::singleShot(0, ...)`, le temps que la mise en page du panneau soit finalisée), au défilement (`QScrollBar::valueChanged`) et au redimensionnement (`resizeEvent`).
  - `ThumbnailCache` plafonné à 300 vignettes en cache (`kMaxCached`), éviction FIFO (la plus ancienne insérée est retirée en premier) au-delà — borne la mémoire même si l'utilisateur parcourt tout un très gros document.
- **Piège rencontré en testant** : un PDF de test à 500 pages généré à la main (xref écrit manuellement) se chargeait correctement selon `QPdfDocument` (nombre de pages détecté) mais `renderPage()` retournait systématiquement une image nulle pour chaque page — défaut du PDF de test, pas du code de Papyrus (confirmé en isolant `Document::renderPage` seul). Régénéré via `PageEditor::mergeFiles` (le vrai moteur PDFium de l'app) pour obtenir un fichier de test garanti valide.
- **Testé réel** : document de 500 pages généré via `PageEditor::mergeFiles`, ouvert dans l'app réelle. Confirmé à l'écran : les pages 1 à ~4 (zone visible du panneau) affichent leur vignette immédiatement, le reste de la liste (jusqu'à Page 500) reste sans icône tant qu'il n'est pas scrollé jusqu'à. Activité CPU du processus mesurée via `/proc/<pid>/stat` : redevient nulle en une fraction de seconde après l'ouverture et reste plate sur plusieurs secondes, confirmant l'absence de rendu de fond prolongé (qu'on observerait si les 500 pages avaient été mises en file).
  - Limite connue : le défilement du panneau lui-même (pour vérifier que de nouvelles vignettes se chargent bien en avançant dans la liste) n'a pas pu être testé interactivement, faute d'outil d'automatisation souris/clavier dans cet environnement — vérifié par lecture de code (le calcul de plage visible est déclenché sur `valueChanged` de la scrollbar) plutôt qu'à l'écran.

## Comment compiler

```bash
scripts/fetch-pdfium.sh          # télécharge PDFium (une fois)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/app/papyrus
```

Dépendances système : `qt6-base-dev`, `qt6-pdf-dev`, `libreoffice-writer` + `libreoffice-impress` (conversion Office), `tesseract-ocr` (+ `tesseract-ocr-fra` pour le français).
