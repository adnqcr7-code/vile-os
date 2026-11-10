# Vile OS — Design Research (Piece 5 reference)

**Purpose:** research only. Vile is a REAL OS — a wlroots Wayland compositor built from scratch — not a website. No mockups were built from this doc; it exists so Piece 5 (top bar, dock, huge desktop clock, Settings layout, lock screen) can steal concrete, proven patterns from real operating systems instead of guessing.

**Vile tokens (fixed):** dark `#0F0B1A`, accent `#6D4AFF`, 14px corner radius everywhere, Inter / JetBrains Mono, huge centred clock, purple Arch wallpaper (`vile-purple-arch.jpg`). Do NOT copy any other OS's branding, logos, or trademarked names.

**Researched:** 2026-11-09/10. Sources are listed at the end.

---

## 1. macOS (Sequoia / Tahoe 26)

Tahoe's new design language is called **Liquid Glass**: translucent, glass-like surfaces that refract the wallpaper behind them. The Dock, menu bar, Control Center and lock screen all share it.

- **Menu bar: transparent by default, ~24–28px tall.** In Tahoe it has no visible background at all — menu text floats over the wallpaper, with only a faint dark gradient behind it for legibility. There is a setting to restore an opaque background (`System Settings → Menu Bar → Show menu bar background`) because text over a light wallpaper gets hard to read. *Steal:* a slim top bar that is mostly transparent/dark, never a solid white strip; keep an opaque fallback.
- **Dock: centred, floating, translucent pill.** Icons sit on a glass surface that lets wallpaper colours shine through; the Dock is a floating rounded rectangle with a gap between it and the screen edge, not glued to the bottom. Icons are large and opaque (so they stay readable over glass), with a small dot under running apps and a magnify-on-hover effect. Launchpad is gone in Tahoe — replaced by an **Apps** icon in the Dock that opens a Spotlight-style grid.
- **Control Center: floating panel, top-right, fully customisable.** Opens from the menu bar; rounded translucent cards, sliders for brightness/volume, toggles for Wi-Fi/Bluetooth/Do Not Disturb. In Tahoe you can add, remove and rearrange controls, including third-party ones.
- **Lock screen: huge clock at the TOP, user at the BOTTOM.** The clock is large and light, with the date above/below it (short date, e.g. "Wed, 16 Sep", no year). Six clock fonts and a weight slider in `System Settings → Wallpaper → Clock Appearance`. At the bottom: a small circular avatar, username, and a glass-pill password field with the caption "Enter Password". Wrong password shakes the field and says so calmly underneath. Sleep/restart/shut-down buttons are available on the same screen.
- **System Settings: left sidebar + content pane.** Categories in a narrow left sidebar (Appearance, Wallpaper, Lock Screen, Users & Groups…), the selected page's controls on the right. Profile picture is changed in **Users & Groups** (photo, emoji, monogram) and that same picture appears on the lock screen — one picture, two places, exactly like Vile's `~/.config/vile/profile.png` + `~/.face`.
- **Icon style: squircle, layered, one size.** All system icons fit the same rounded-square (squircle) shape with a subtle light edge. Third-party icons that don't fit get shrunk onto a grey squircle ("shame box") — the lesson is: enforce one icon shape/size everywhere or the dock looks messy.
- **Volume/brightness feedback: small pill overlay, top-right.** Tahoe replaced the old huge centre-screen overlays with a small oblong overlay under the menu bar, notification-style.

## 2. Windows 11 (Fluent Design)

Fluent = rounded corners everywhere, layered translucency (**Mica** material — wallpaper tint showing faintly through panels), soft shadows, calm motion.

- **Taskbar: bottom, centred by default, ~48px tall.** Start button + pinned apps centred; system tray (Wi-Fi, volume, battery, clock) at the far right. Alignment can switch Left/Centre. Icons shrink automatically when space runs out; a compact mode exists. Auto-hide is an option so the wallpaper gets the full screen.
- **Start menu: centred floating panel, rounded corners.** Pinned apps in a grid (Live Tiles are gone), a Recommended section of recent files below, a search box at the top, and an All Apps list. Power + user buttons sit at the bottom of the panel. It floats above the desktop with a gap, not full-screen.
- **Settings app: left navigation pane on EVERY page + breadcrumbs.** Categories (System, Bluetooth & devices, Personalization, Accounts…) always visible on the left; the right side shows the page, with a live preview at the top on personalisation pages (e.g. the lock-screen page shows a preview of the lock screen that updates as you change it). One accent colour is used everywhere (Start, taskbar, title bars) when "Show accent color" is on.
- **Quick Settings: one click, one panel.** Clicking the network/volume/battery cluster (or Win+A) opens a panel with Wi-Fi, Bluetooth, brightness, volume sliders and a power button — the same "fix it fast" panel ChromeOS has. No digging through the full Settings app for the common stuff.
- **Lock screen vs sign-in screen: two steps.** Lock screen = full-screen picture with a **large bold time + smaller date, centred at the top** (moved from bottom-left in Windows 10 to centre-top in 11, like a phone). Press a key / click / swipe up → sign-in screen: same background, circular profile picture + username centred, rounded password/PIN field with a solid underline. Optional widgets (weather) sit centred at the bottom of the lock screen.
- **Snap layouts:** hovering the maximise button shows window-arrangement layouts (halves, quarters). Windows snap side by side with a thin gap and a shared accent border on the active window — the closest mainstream equivalent to Hyprland tiling.
- **Corners: 8px rounded on windows, menus and fields.** Everything is slightly rounded, never sharp 90° corners and never fully round — a consistent mid radius is what makes it feel calm.

## 3. GNOME (Ubuntu / Fedora)

GNOME's whole idea: the desktop is empty and clean; almost nothing lives on it permanently.

- **Top bar: one thin black bar (~32px), three zones.** Left: "Activities" button. Centre: clock + date (click it for calendar + notifications). Right: status icons (network, volume, battery) that open one system menu. The bar is dark and mostly transparent over the wallpaper; on the lock screen it goes fully transparent. *Steal:* three-zone bar (left = launcher/workspaces, centre = clock, right = status) — Vile's top bar should copy this layout.
- **Activities overview: the launcher IS the overview.** Press Super (Windows key) or click Activities: open windows shrink into a grid in the centre, a search box sits at the top, and the dock (dash) appears at the bottom centre with favourites + running apps. No separate Start menu — search + window previews in one screen.
- **Dash/dock: bottom centre, only in the overview (Ubuntu) or left side (vanilla GNOME).** Favourite apps pinned; a small dot under each running app. Hover shows the app name. The desktop itself shows no icons and no files — dragging files onto the desktop is impossible by design.
- **Settings: left sidebar categories, right-side pages.** Same sidebar pattern as macOS/Windows. Appearance page controls wallpaper, lock-screen image and light/dark theme in one place. Settings is opened from the top-right system menu or the Activities search.
- **Lock screen: two stages.** First a "curtain" — just a huge clock over the wallpaper, nothing else. Press a key / move the mouse → the unlock dialog slides up: user avatar, username, password field. After a while idle it goes fully black. The lesson: the clock-only curtain is the calmest possible lock screen; the password field only appears when the user acts.
- **System menu (top-right): one menu for everything.** Volume + brightness sliders, Wi-Fi network picker, power off / restart / lock / log out, and a Settings shortcut — all in one dropdown. Vile's top-bar right side should be exactly this, not five separate buttons.
- **Minimal chrome, generous spacing.** GNOME uses a lot of empty space, small (13–14px) text, and only one accent colour. It feels "expensive" because it shows very little at once.

## 4. KDE Plasma 6

Plasma is the opposite of GNOME: everything is configurable, but the defaults are deliberately familiar (bottom panel, app menu, system tray).

- **Panel: bottom by default, ~36–48px, fully movable.** Left: Application Launcher (logo button). Middle: task manager — pinned apps + open windows, with a dot/underline for running ones. Right: system tray (network, volume, battery, Bluetooth, clipboard, notifications) + clock. Right-click the panel → "Show Panel Configuration" to move it to any edge, resize it, or add widgets (weather, system monitor). *Steal:* the panel is a set of widgets in three zones, not one fixed bar — Vile can do the same with its top bar + dock.
- **Application Launcher: full menu, search-first.** Opens at Favourites; start typing and it filters instantly (KRunner, Alt+Space, does the same from anywhere). The bottom row of the launcher always shows Lock, Log Out, Switch User, Sleep, Restart, Shut Down — power actions are never hidden in a submenu.
- **System Settings: one place, nine categories, search at the top.** Left sidebar with categories (Appearance, Workspace, Network, Hardware, System…); changed settings get an orange dot so you can see what you touched. A search field at the top-left finds any setting by name — this is the pattern Vile Settings should copy once it grows past Profile Picture + Wallpaper.
- **Lock screen: same wallpaper as the desktop, centred clock + user card.** KScreenLocker shows the wallpaper (same image as the desktop, for visual continuity), a clock, the user avatar + name, and the password field. It also lets you pause/play media from the lock screen, and it respawns itself if it crashes so a crash never leaves the session unlocked.
- **Screen locking that respects the user:** auto-lock after N minutes of inactivity, plus a 30-second grace period where moving the mouse cancels the lock — no password needed if you were only gone for a second. Vile should consider a short grace period too.
- **Widgets everywhere, but restrained by default.** The desktop ships clean; widgets (weather, notes, monitor) are opt-in. The lesson: ship Vile clean, let the user add things, don't pre-clutter the desktop.
- **Floating panel option:** Plasma 6's default panel floats with a small gap from the screen edge and rounded corners — the same floating-pill idea as macOS's Dock, and the reason both look modern while a glued-on bar looks old.

## 5. Hyprland / popular Hyprland rices (ML4W, End-4, JaKooLit, Omarchy, Caelestia)

Hyprland itself is only a compositor — the "look" comes from the rice (dotfiles). The best rices share one rule: **every surface uses the same tokens** (wallpaper-derived palette, one font, one radius, one border).

- **Waybar: top bar, 26–48px, three zones, often floating.** Left: logo button (opens the launcher) + workspace pills (numbered 1–0, active one filled with the accent colour). Centre: clock + date. Right: network, volume, battery, tray, power. The best rices make it a floating bar with 8–14px margins from the screen edge and rounded corners — never edge-to-edge. Some use three separate "islands" (left/centre/right pills) instead of one long bar.
- **Gaps + rounded corners + one accent border.** The signature Hyprland look: 7–14px gaps between windows, 12px window rounding, and a solid accent border (often a gradient) ONLY on the focused window. Unfocused windows get a dim grey border. This is exactly what Piece 4 is building — and it's the single most recognisable "rice" cue.
- **Launcher: centred, floating, search-first (wofi / rofi / Walker / fuzzel).** A small centred window (~600px wide) with a search box at the top and app results below; typing filters instantly; Enter launches. Styled to match the bar (same background, same accent for the selected row). Rofi themes are usually a dark translucent card with a coloured left edge on the selected row.
- **hyprlock: blurred wallpaper + huge clock + centred password field.** The standard recipe: screenshot the desktop, blur it (often 3-pass Gaussian), big digital clock in a mono or light font, date under it, password field in a small centred card with the accent border. Lock-screen avatar comes from `~/.face` — the same file Vile already uses for the profile picture. Some rices add a "LOCKED / hostname" label and key hints under the field.
- **Control centre: a slide-in panel, not a window.** Clicking the network/volume pill in Waybar opens a panel (often Quickshell or a rofi menu) with Wi-Fi, Bluetooth, sound devices, dark/light toggle, Do Not Disturb, Night Light, power profiles. Vile's lock-screen settings panel and a future top-bar control centre should copy this: quick toggles in one place, full Settings only for the rest.
- **One palette derived from the wallpaper (matugen / Wallust).** The best rices pick a wallpaper, then generate the bar, terminal, lock screen, launcher and GTK theme colours FROM that wallpaper automatically (Material You style). Vile's fixed purple palette is the simpler version of the same idea: one accent, used everywhere, never a second accent.
- **Animations are short and purposeful.** Windows fade/slide in over ~150–250ms; workspaces slide. Nothing bounces, nothing takes a second. The lesson for Vile: add small, fast animations last — they make a plain desktop feel expensive, but only if everything else is already clean.

## 6. ChromeOS

ChromeOS is the simplest desktop of the six — and the best reference for "a kid should understand it in 5 seconds".

- **Shelf: bottom strip, like a taskbar + dock in one.** Left: Launcher button (circle). Middle: pinned apps + running apps (dot underneath running ones). Right: status area (clock, battery, Wi-Fi). It can move to the left/right edge or auto-hide (slides away until you hover the edge). Pin an app by right-clicking it in the Launcher → "Pin to shelf".
- **Launcher: search box + recent apps first, full-screen grid second.** Click the Launcher (or the Search key): a panel with a search box at the top and recently opened apps. Click the up-arrow / drag up → full-screen grid of every app, organised into folders by drag-and-drop. Search finds apps, files AND settings in one box.
- **Quick Settings: one panel from the status area.** Click the clock/battery/Wi-Fi cluster → a small panel with Wi-Fi, Bluetooth, volume, brightness, accessibility, plus Lock, Sign out, Shut down and a Settings shortcut. Three urgent questions answered in one place: "Am I connected? How much battery? Why is there no sound?"
- **Settings: searchable categories, profile picture under Personalization.** Settings is a web-style page with a left category list and a search box; changing the account picture is under Personalization → "Change device account image", and the lock screen previews it. Same one-picture-two-places pattern as macOS and Vile.
- **Lock screen: minimal, ambient optional.** By default just the clock + user + PIN/password field over the wallpaper. Ambient mode can run a photo slideshow as the lock background. There is deliberately very little to customise — the lock screen is for locking, not for decorating.
- **Everything is a rounded card, one font, one accent.** ChromeOS uses generous corner radius (12–16px), soft shadows and a single system font. It looks friendly because nothing is sharp and nothing is crowded.

---

## What Vile should steal for Piece 5

Concrete, in build order:

1. **Top bar — GNOME's three zones + macOS transparency.** Slim (~32px), mostly transparent/dark over the wallpaper: left = Vile logo button + workspaces, centre = clock + date, right = network/volume/battery/power in one menu (GNOME system menu). Never a solid white bar (the current Settings header bug is the warning).
2. **Dock — macOS floating pill + Plasma's running-app dots.** Bottom centre, floating with a gap from the edge, translucent dark, rounded 14px+; app icons ~48–56px, opaque, one shape (use the foot logo's dark tile + the Vile V); a small purple dot under running apps; click a running app to focus it, right-click to pin/unpin.
3. **Huge desktop clock — macOS lock-screen clock + Hyprland hyprlock.** Big light Inter clock centred on the desktop, date under it, over the wallpaper only (no box). Keep it separate from the lock-screen clock — same component, two places.
4. **Settings layout — every OS agrees: left sidebar + right page.** Vile Settings already has Profile Picture first; add a narrow left sidebar (Profile, Wallpaper, Display/ScreenFit, Sound, About) once there are more sections, a search box at the top when it grows, and a live preview at the top of the Wallpaper page. Keep Profile Picture first.
5. **Lock screen — Windows 11 + macOS consensus.** Huge clock at the top, date under it; user card in the middle (circular avatar from `~/.config/vile/profile.png`, username, LOCKED, password field); power buttons + settings gear at the bottom corners; blurred wallpaper only behind — Vile already does this, keep it. Add GNOME's idea: a clock-only curtain first, password field appearing when the user acts (optional, later).
6. **Quick panel — Windows Quick Settings + ChromeOS Quick Settings.** Clicking the top-bar status cluster opens one small floating panel: Wi-Fi, volume, brightness, Lock, Power off, Settings shortcut. This is more useful than a full Settings window for the 5 things people touch daily.
7. **Launcher — Hyprland wofi/rofi, centred, search-first.** Super key or dock button: small centred floating window, search box on top, results below, accent highlight on the selected row, Enter to launch. Same dark card style as everything else.
8. **Window rules — Hyprland gaps + one accent border.** 8–12px gaps, 14px corners (Vile's token), accent `#6D4AFF` border only on the focused window, dim border elsewhere. This is Piece 4's job; Piece 5 must not undo it with a second border style.
9. **One icon shape, one font, one accent.** Every icon in the dock/launcher must be the same size and same rounded-square treatment; Inter everywhere, JetBrains Mono only in the terminal/clock numbers if needed. No second accent colour — the fastest way to look cheap is two accents.

## What Vile should NOT copy

- **macOS Liquid Glass overdone:** full transparency everywhere makes text unreadable over a bright wallpaper — Tahoe itself had to add an opaque-menu-bar toggle and a transparency slider. Vile's bars should be dark and mostly opaque; use blur sparingly, only on the lock screen.
- **Windows 11's two screens for one job:** a separate lock screen AND sign-in screen (click to get from one to the other) is an extra step for nothing. Vile's one-screen lock (clock + user + field together) is better — keep it.
- **GNOME's "no desktop icons, no customisation" rigidity:** users should be allowed to change the wallpaper, the dock position and the accent-adjacent choices. Vile is Adnan's OS; it should feel owned, not locked down.
- **KDE's everything-configurable sprawl:** nine settings categories with an orange "changed" dot on everything is powerful but overwhelming. Vile Settings should stay small: Profile, Wallpaper, Display, Sound, About — and stop there for now.
- **Hyprland rices' neon overload:** gradient borders, glowing everything, animated wallpapers and 6 different accent colours look great in a screenshot and tiring after a day. Vile is one purple, one dark, done.
- **ChromeOS's "the browser is the OS":** Vile is a real desktop OS; apps are native windows, not browser tabs. Never turn a Vile surface into a web page.
- **Any other OS's branding:** no Apple logo, no Windows logo, no GNOME foot, no KDE gear, no Chrome colours. Vile's mark is the purple V; foot's mark is the blue footprint + `>_` + "foot".

---

## Sources

Articles / docs (read via search, Nov 9–10, 2026):

- macOS Tahoe design: "macOS Tahoe Guide: Every Feature, Tip and Trick" — https://www.techbetweenthelines.com/macos-26-tahoe-guide-the-productivity-revolution-reaches-the-mac/
- macOS Tahoe customisation: "Six New Ways to Customize Your Mac's Look in macOS Tahoe" — https://europennews.com/six-new-ways-to-customize-your-macs-look-in-macos-tahoe/
- macOS Tahoe review: "macOS Tahoe: The MacStories Public Beta Preview" — https://www.macstories.net/stories/macos-tahoe-the-macstories-public-beta-preview/
- macOS Tahoe features: "50 New macOS Tahoe Features and Changes Worth Checking Out" — http://en.zicos.com/mac/i32301059-50-New-macOS-Tahoe-Features-and-Changes-Worth-Checking-Out.html
- macOS lock screen: "How to customize your Mac's lock screen (8 tips)" — https://www.idownloadblog.com/2020/05/20/customize-login-screen-mac/
- macOS lock screen clock: "How to Change the Lock Screen on Mac" — https://cindori.com/how-to/how-to-change-mac-lock-screen
- macOS lock-screen reference design (Lisa OS login docs, modelled on macOS): https://docs.lisaos.dev/system/login
- Windows 11 design: "Windows 11 Features List: The Complete 2026 Guide" — https://www.tutorialpedia.org/blog/windows-11-features-list/
- Windows 11 lock + sign-in screens: "Customize the Lock and Sign-In Screens" (Thurrott) — https://www.thurrott.com/books/windows-11-field-guide/personalize-windows-11/337582/customize-the-lock-and-sign-in-screens-2
- Windows 11 lock screen: "What's new with the Lock screen on Windows 11" — https://www.windowscentral.com/whats-new-lock-screen-windows-11
- Windows 11 home screen / lock / Start: "How to Change the Home Screen on Windows 11" — https://sekin.in/how-to-change-home-screen-on-windows-11/
- GNOME desktop: "Getting Started with GNOME Desktop" (SUSE) — https://doc.suse.com/sles/16.0/pdf/SLES-GNOME-getting-started_en.pdf
- GNOME user guide (lock screen curtain): https://documentation.suse.com/en-us/sles/15-SP6/html/SLES-all/cha-gnomeuser-start.html
- GNOME desktop layout (top bar, dock, Activities): https://ecampusontario.pressbooks.pub/otutechtangerineguides/wp-json/pressbooks/v2/chapters/133
- KDE Plasma desktop: "The Desktop — LeiOS Docs" — https://www.leios.dev/docs/usage/desktop
- KDE Plasma launcher/settings/lock: "Top Five reasons to use KDE Plasma desktop" — https://www.both.org/?p=7092
- KDE Plasma 6 config (screen locker, KRunner): https://deepwiki.com/dtomvan/puntbestanden/5.1-kde-plasma-configuration
- Hyprland rices: "Omarchy: Complete Features Guide" — https://github.com/ivokun/kebun/blob/HEAD/docs/omarchy/omarchy-features-guide.md
- Hyprland rice (Enoshima): "Desktop expansion design" — https://github.com/shiinamachi/enoshima/blob/HEAD/docs/DESKTOP-EXPANSION.md
- Hyprland rice (tjabo dotfiles, Waybar/hyprlock deck style): https://www.tjabo.dev/projects/dotfiles
- Hyprland dotfiles roundup (ML4W, End-4, HyDE, Caelestia): "10 Hyprland Dotfiles to Transform Your Linux Desktop" — https://itsfoss.com/best-hyprland-dotfiles/
- Hyprland rice (CachyOS, matugen palette, hyprlock): https://github.com/huzaifahshahid71-ops/hyprland-cachyos-rice/blob/HEAD/README.md
- Hyprland overview: https://grokipedia.com/page/hyprland
- Hyprland → GNOME comparison (hyprlock blur + big clock recipe): https://blog.jrodriiguezg.link/en/blog/de-gnome-a-hyperland/
- ChromeOS desktop: "Using Chrome OS and the Chrome Desktop" — https://www.informit.com/articles/article.aspx?p=2982121&seqNum=2
- ChromeOS basics (shelf, launcher, status tray): https://subscription.packtpub.com/book/cloud-and-networking/9781803241050/2/ch02lvl1sec06/personalizing-chromeos
- ChromeOS guide: "A Beginner's Guide to ChromeOS" — https://www.howtogeek.com/beginners-guide-to-chromeos/

Videos (searched, descriptions/transcripts where available — not watched in a browser, noted as such):

- YouTube: "macOS Tahoe (26) is AMAZING! Do these 10 things FIRST!" — transcript at https://youtubetotranscript.com/transcript?v=kAlAIOum0JA (Liquid Glass, floating Control Center, customisable menu bar, Apps replacing Launchpad)
- YouTube (TWiT): "Hands-On Apple: macOS Tahoe 26" — https://www.youtube.com/embed/kUhT77FtpMw (Liquid Glass walkthrough, Control Center, icon styles)
- MacStories: "macOS 26 Tahoe: The MacStories Review" (includes video) — https://www.macstories.net/stories/macos-26-tahoe-the-macstories-review/2/ (Dock, App Switcher, transparent menu bar)
- YouTube: "NEW Ubuntu 26.04 HYPRLAND Setup: MAKE Linux BEAUTIFUL (2026)" — https://www.youtube.com/watch?v=sSH8nlsGUM4 (Hyprland, Waybar, rofi, blur)
- YouTube: "THE REAL MINDBLOWING ARCH LINUX HYPRLAND SETUP 2025 (Ft. ML4W DotFiles)" — https://www.youtube.com/watch?v=Nx-Y3I58D58 (ML4W sidebar, Waybar, wallpapers)
- YouTube: "THE NEW HYPRLAND Setup Will BLOW Your Mind! (Dux Shell 2.1)" — https://www.youtube.com/watch?v=ol4gNFsL9r0 (top bar, system settings, Dux Dash)
- YouTube: "THE BEST ENDEAVOUR OS HYPRLAND SETUP 2026 (FT. NEW END4 DotFiles)" — https://www.youtube.com/watch?v=egi6a3uPEIA (End-4 dotfiles, Hyprland desktop)
- YouTube: "Rice Hyprland in 2 Hours | Full Guide" — https://www.youtube.com/watch?v=ftHfRmtqDTU (full ricing walkthrough)
- YouTube: "HYPRLAND RICE FRUTIGER AERO ON LINUX" — https://www.youtube.com/watch?v=na4bAxvL8V8 (Waybar CSS, wofi styling)

*Note: video entries above are from search results and their published descriptions/transcripts. No video was played in a browser; the design claims in this doc rest on the articles/docs, with the videos used only as pointers to what to watch for Piece 5.*
