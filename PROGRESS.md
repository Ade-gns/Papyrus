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

## Prochaines étapes (Phases 8-9, non commencées)

- **Phase 8** : impression (natif Linux/CUPS), historique Undo/Redo multi-niveaux + sauvegarde automatique + récupération après crash, optimisation des performances (gros documents, chargement à la demande).
- **Phase 9** : packaging — `.deb`, `.rpm`, AppImage, Flatpak, script d'installation en une commande, système de mise à jour.

## Comment compiler

```bash
scripts/fetch-pdfium.sh          # télécharge PDFium (une fois)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/app/papyrus
```

Dépendances système : `qt6-base-dev`, `qt6-pdf-dev`, `libreoffice-writer` + `libreoffice-impress` (conversion Office), `tesseract-ocr` (+ `tesseract-ocr-fra` pour le français).
