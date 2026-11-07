-- Delta Executor UI — full Lua UI using all bundled assets.
-- This is the executor's UI definition. It runs inside the executor's
-- Luau VM and uses the host's UI primitives (exposed via the native
-- dispatch / Obj-C host) to render the interface.
--
-- Assets used (from Resources/assets/):
--   Icons: Home.png, Settings.png, Execute.png, Play.png, Console.png,
--          Discord.png, Telegram.png, Cloud.png, Key.png, File.png,
--          LocalFile.png, Delete.png, Save.png, Open.png, Clear.png,
--          Search.png, Filter.png, Clock.png, Timer.png, Battery.png,
--          Wifi.png, Website.png, Clipboard.png, TextWindow.png,
--          Dropdown.png, Star.png, Tick.png, Verified.png, Unverified.png,
--          Info.png, Warning.png, WarningRed.png, Error.png, SuccessNotify.png,
--          ErrorNotify.png, SheildNotify.png, AutoExecNotify.png,
--          InfoNotify.png, Controller.png, Views.png, Close.png,
--          Erase.png, Arrow.png, TallArrow.png, Bolt.png, SunLarge.png,
--          SunSmall.png, AddFile.png, FileDownload.png, FilenClipboard.png,
--          OldUI.png, NewUI.png, Logo (logo.png)
--   Backgrounds: NewUI.png (1920x1080), OldUI.png (1920x1080)
--   Fonts: Montserrat (f1), Poppins (Poppins.rbxmx)

-- ============================================================
-- Asset catalog — maps logical names to asset file names.
-- The host resolves these to bundle resources at runtime.
-- ============================================================

local Assets = {
    -- Navigation / main
    Home           = "Home.png",
    Settings       = "Settings.png",
    Execute        = "Execute.png",
    Play           = "Play.png",
    Console        = "Console.png",
    Discord        = "Discord.png",
    Telegram       = "Telegram.png",
    Cloud          = "Cloud.png",
    Key            = "Key.png",
    File           = "File.png",
    LocalFile      = "LocalFile.png",
    Delete         = "Delete.png",
    Save           = "Save.png",
    Open           = "Open.png",
    Clear          = "Clear.png",
    Search         = "Search.png",
    Filter         = "Filter.png",
    Clock          = "Clock.png",
    Timer          = "Timer.png",
    Battery        = "Battery.png",
    Wifi           = "Wifi.png",
    Website        = "Website.png",
    Clipboard      = "Clipboard.png",
    TextWindow     = "TextWindow.png",
    Dropdown       = "Dropdown.png",
    Star           = "Star.png",
    Tick           = "Tick.png",
    Close          = "Close.png",
    Erase          = "Eraser.png",
    Arrow          = "Arrow.png",
    TallArrow      = "TallArrow.png",
    Bolt           = "Bolt.png",
    AddFile        = "AddFile.png",
    FileDownload   = "FileDownload.png",
    FilenClipboard = "FilenClipboard.png",

    -- Status / verification
    Verified       = "Verified.png",
    Unverified     = "Unverified.png",
    Info           = "Info.png",
    Warning        = "Warning.png",
    WarningRed     = "WarningRed.png",
    Error          = "Error.png",
    SuccessNotify  = "SuccessNotify.png",
    ErrorNotify    = "ErrorNotify.png",
    SheildNotify   = "SheildNotify.png",
    AutoExecNotify = "AutoExecNotify.png",
    InfoNotify     = "InfoNotify.png",
    Controller     = "Controller.png",
    Views          = "Views.png",

    -- UI themes (full-screen backgrounds)
    NewUI = "NewUI.png",
    OldUI = "OldUI.png",

    -- Logo
    Logo = "logo.png",

    -- Sun / decorative
    SunLarge = "SunLarge.png",
    SunSmall = "SunSmall.png",

    -- Notification icons grouped
    Notifications = {
        Success = "SuccessNotify.png",
        Error   = "ErrorNotify.png",
        Warning = "Warning.png",
        Info    = "Info.png",
        Shield  = "SheildNotify.png",
        AutoExec = "AutoExecNotify.png",
        InfoNotify = "InfoNotify.png",
    },

    -- All icon strip images (i1..i18, f1)
    -- These are the individual icon strip images from the asset bundle.
    IconStrip = {
        i1, i2, i3, i4, i5, i6, i7, i8, i9,
        i11, i12, i13, i14, i15, i16, i17, i18,
    },
}

-- ============================================================
-- Font catalog — from f1 (Montserrat) and Poppins.rbxmx
-- ============================================================

local Fonts = {
    Montserrat = {
        Thin          = "rbxassetid://11598116985",
        ["Thin Italic"] = "rbxassetid://11598120684",
        ["Extra Light"] = "rbxassetid://11598119328",
        ["Extra Light Italic"] = "rbxassetid://11598116110",
        Light         = "rbxassetid://11598118107",
        ["Light Italic"] = "rbxassetid://11598116400",
        Regular       = "rbxassetid://11598116646",
        Italic        = "rbxassetid://11598117311",
        Medium        = "rbxassetid://11598120262",
        ["Medium Italic"] = "rbxassetid://11598115824",
        ["Semi Bold"] = "rbxassetid://11598119872",
        ["Semi Bold Italic"] = "rbxassetid://11598117539",
        Bold          = "rbxassetid://11598115314",
        ["Bold Italic"] = "rbxassetid://11598117769",
        ["Extra Bold"] = "rbxassetid://11598118540",
        ["Extra Bold Italic"] = "rbxassetid://11598118921",
        Black         = "rbxassetid://11598119594",
        ["Black Italic"] = "rbxassetid://11598119154",
    },
    Poppins = {
        Thin          = "rbxassetid://11598354471",
        ["Thin Italic"] = "rbxassetid://11598352697",
        ["Extra Light"] = "rbxassetid://11598356856",
        ["Extra Light Italic"] = "rbxassetid://11598356464",
        Light         = "rbxassetid://11598353653",
        ["Light Italic"] = "rbxassetid://11598352688",
        Regular       = "rbxassetid://11598355136",
        Italic        = "rbxassetid://11598355212",
        Medium        = "rbxassetid://11598355812",
        ["Medium Italic"] = "rbxassetid://11598353770",
        Semibold      = "rbxassetid://11598353514",
        ["Semibold Italic"] = "rbxassetid://11598353990",
        Bold          = "rbxassetid://11598355670",
        ["Bold Italic"] = "rbxassetid://11598355334",
        ["Extra Bold"] = "rbxassetid://11598355876",
        ["Extra Bold Italic"] = "rbxassetid://11598355528",
        Black         = "rbxassetid://11598355633",
        ["Black Italic"] = "rbxassetid://11598355238",
    },
}

-- ============================================================
-- UI State
-- ============================================================

local UI = {
    Theme = "NewUI",  -- "NewUI" or "OldUI"
    Visible = true,
    Window = nil,
    Tabs = {},
    ConsoleOutput = {},
    Scripts = {},
    AutoExecute = {},
}

-- ============================================================
-- UI Builder — constructs the UI using host primitives.
-- In a real executor these would call into the Obj-C/Swift host
-- via the native dispatch to create views, buttons, labels, etc.
-- ============================================================

local function create_window(title, width, height, background)
    -- Creates the main executor window.
    -- Host call: create UIWindow with title, size, background image.
    -- Background: Assets.NewUI (1920x1080) or Assets.OldUI.
    local bg = background or Assets[UI.Theme]
    UI.Window = {
        Title = title,
        Width = width or 1920,
        Height = height or 1080,
        Background = bg,
        Components = {},
    }
    return UI.Window
end

local function add_tab(name, icon, callback)
    -- Adds a navigation tab (Home, Settings, Execute, Console, etc.)
    local tab = {
        Name = name,
        Icon = icon and Assets[icon] or nil,
        Callback = callback,
        Visible = true,
    }
    table.insert(UI.Tabs, tab)
    return tab
end

local function add_button(parent, text, icon, action)
    -- Adds a UI button with optional icon.
    local btn = {
        Text = text,
        Icon = icon and Assets[icon] or nil,
        Action = action,
        Enabled = true,
    }
    if parent then
        parent.Components = parent.Components or {}
        table.insert(parent.Components, btn)
    end
    return btn
end

local function add_label(parent, text, font, size, color)
    local lbl = {
        Text = text,
        Font = font or "Montserrat",
        FontWeight = "Regular",
        Size = size or 14,
        Color = color or "0xFFFFFFFF",
    }
    if parent then
        parent.Components = parent.Components or {}
        table.insert(parent.Components, lbl)
    end
    return lbl
end

local function add_console(parent)
    -- Adds the script console output window.
    local console = {
        Type = "Console",
        Icon = Assets.Console,
        Output = {},
        Font = "Montserrat",
        FontWeight = "Regular",
        Size = 12,
        Background = "0xFF1A1A1A",
        Color = "0xFFCCCCCC",
    }
    if parent then
        parent.Components = parent.Components or {}
        table.insert(parent.Components, console)
    end
    UI.ConsoleOutput = console
    return console
end

local function add_script_editor(parent)
    -- Adds the script editor / execution window.
    local editor = {
        Type = "TextWindow",
        Icon = Assets.TextWindow,
        Font = "Poppins",
        FontWeight = "Regular",
        Size = 14,
        Background = "0xFF1A1A1A",
        Color = "0xFFCCCCCC",
    }
    if parent then
        parent.Components = parent.Components or {}
        table.insert(parent.Components, editor)
    end
    return editor
end

local function add_notification(parent, type, message)
    -- Shows a notification (success, error, warning, info).
    local notify_icon = {
        Success = Assets.SuccessNotify,
        Error   = Assets.ErrorNotify,
        Warning = Assets.Warning,
        Info    = Assets.InfoNotify,
        Shield  = Assets.SheildNotify,
        AutoExec = Assets.AutoExecNotify,
    }[type] or Assets.InfoNotify

    local notification = {
        Type = type,
        Icon = notify_icon,
        Message = message,
        Duration = 3000,
    }
    if parent then
        parent.Components = parent.Components or {}
        table.insert(parent.Components, notification)
    end
    return notification
end

-- ============================================================
-- Build the full executor UI
-- ============================================================

local function build_ui()
    -- Main window with NewUI background
    local main = create_window("Delta Executor", 1920, 1080, Assets.NewUI)

    -- Top bar
    add_label(main, "DELTA", "Montserrat", 28, "0xFFFFFFFF")
    add_button(main, "Home", "Home", function() end)
    add_button(main, "Settings", "Settings", function() end)
    add_button(main, "Execute", "Execute", function() end)
    add_button(main, "Console", "Console", function() end)
    add_button(main, "Discord", "Discord", function() end)
    add_button(main, "Telegram", "Telegram", function() end)
    add_button(main, "Cloud", "Cloud", function() end)
    add_button(main, "Close", "Close", function()
        UI.Visible = false
    end)

    -- Sidebar / navigation icons
    local nav = {
        Components = {},
    }
    add_label(nav, "NAVIGATION", "Montserrat", 12, "0xFF888888")
    add_button(nav, "Home", "Home")
    add_button(nav, "Scripts", "File")
    add_button(nav, "Auto Exec", "Play")
    add_button(nav, "Settings", "Settings")
    add_button(nav, "Key", "Key")
    add_button(nav, "Timer", "Timer")
    add_button(nav, "Battery", "Battery")
    add_button(nav, "Wifi", "Wifi")
    add_button(nav, "Website", "Website")
    add_button(nav, "Clipboard", "Clipboard")
    add_button(nav, "Views", "Views")
    add_button(nav, "Controller", "Controller")
    add_button(nav, "Star", "Star")
    add_button(nav, "Cloud", "Cloud")
    add_button(nav, "Bolt", "Bolt")
    add_button(nav, "SunLarge", "SunLarge")
    add_button(nav, "SunSmall", "SunSmall")
    add_label(nav, "COMMANDS", "Montserrat", 12, "0xFF888888")
    add_button(nav, "Execute", "Execute")
    add_button(nav, "Play", "Play")
    add_button(nav, "Save", "Save")
    add_button(nav, "Open", "Open")
    add_button(nav, "Delete", "Delete")
    add_button(nav, "Clear", "Clear")
    add_button(nav, "Search", "Search")
    add_button(nav, "Filter", "Filter")
    add_button(nav, "Clipboard", "Clipboard")
    add_button(nav, "TextWindow", "TextWindow")
    add_button(nav, "Dropdown", "Dropdown")
    add_button(nav, "AddFile", "AddFile")
    add_button(nav, "FileDownload", "FileDownload")
    add_button(nav, "FilenClipboard", "FilenClipboard")
    add_button(nav, "Erase", "Eraser")
    add_button(nav, "Arrow", "Arrow")
    add_button(nav, "TallArrow", "TallArrow")
    add_button(nav, "Close", "Close")
    add_button(nav, "Clock", "Clock")
    add_button(nav, "Timer", "Timer")

    -- Status bar with verification indicators
    local status = {
        Components = {},
    }
    add_label(status, "STATUS", "Montserrat", 12, "0xFF888888")
    add_button(status, "Verified", "Verified")
    add_button(status, "Unverified", "Unverified")
    add_button(status, "Info", "Info")
    add_button(status, "Warning", "Warning")
    add_button(status, "WarningRed", "WarningRed")
    add_button(status, "Error", "Error")
    add_button(status, "ErrorNotify", "ErrorNotify")
    add_button(status, "SuccessNotify", "SuccessNotify")
    add_button(status, "SheildNotify", "SheildNotify")
    add_button(status, "AutoExecNotify", "AutoExecNotify")
    add_button(status, "InfoNotify", "InfoNotify")

    -- Console output
    add_console(main)

    -- Script editor
    add_script_editor(main)

    -- Theme toggle (NewUI / OldUI backgrounds)
    add_button(main, "Theme: " .. UI.Theme, "Dropdown", function()
        if UI.Theme == "NewUI" then
            UI.Theme = "OldUI"
        else
            UI.Theme = "NewUI"
        end
        if UI.Window then
            UI.Window.Background = Assets[UI.Theme]
        end
        add_notification(main, "Info", "Theme changed to " .. UI.Theme)
    end)

    -- Logo in corner
    add_label(main, "LOGO", "Montserrat", 10, "0xFF888888")

    -- All icon strip images as a gallery
    local gallery = {
        Components = {},
    }
    add_label(gallery, "ICON STRIP", "Montserrat", 12, "0xFF888888")
    for _, iconName in ipairs(Assets.IconStrip) do
        add_button(gallery, iconName, iconName)
    end
    if main then
        main.Components = main.Components or {}
        table.insert(main.Components, gallery)
    end

    UI.Window = main
    return main
end

-- ============================================================
-- Navigation tabs
-- ============================================================

local function build_tabs()
    add_tab("Home", "Home", function()
        add_notification(UI.Window, "Info", "Welcome to Delta Executor")
    end)
    add_tab("Execute", "Execute", function()
        add_notification(UI.Window, "Success", "Executor ready")
    end)
    add_tab("Scripts", "File", function()
        add_notification(UI.Window, "Info", "Script manager opened")
    end)
    add_tab("Auto Exec", "Play", function()
        add_notification(UI.Window, "AutoExec", "Auto-execution enabled")
    end)
    add_tab("Console", "Console", function()
        add_notification(UI.Window, "Info", "Console opened")
    end)
    add_tab("Settings", "Settings", function()
        add_notification(UI.Window, "Info", "Settings opened")
    end)
    add_tab("Discord", "Discord", function()
        add_notification(UI.Window, "Info", "Discord link opened")
    end)
    add_tab("Telegram", "Telegram", function()
        add_notification(UI.Window, "Info", "Telegram link opened")
    end)
    add_tab("Cloud", "Cloud", function()
        add_notification(UI.Window, "Info", "Cloud storage opened")
    end)
    add_tab("Key", "Key", function()
        add_notification(UI.Window, "Info", "Key manager opened")
    end)
    add_tab("Timer", "Timer", function()
        add_notification(UI.Window, "Info", "Timer started")
    end)
    add_tab("Battery", "Battery", function()
        add_notification(UI.Window, "Info", "Battery: monitoring")
    end)
    add_tab("Wifi", "Wifi", function()
        add_notification(UI.Window, "Info", "WiFi: connected")
    end)
    add_tab("Website", "Website", function()
        add_notification(UI.Window, "Info", "Website opened")
    end)
    add_tab("Clipboard", "Clipboard", function()
        add_notification(UI.Window, "Info", "ClipboardManager opened")
    end)
    add_tab("Views", "Views", function()
        add_notification(UI.Window, "Info", "Views opened")
    end)
    add_tab("Controller", "Controller", function()
        add_notification(UI.Window, "Info", "Controller opened")
    end)
    add_tab("Star", "Star", function()
        add_notification(UI.Window, "Info", "Starred scripts")
    end)
    add_tab("Bolt", "Bolt", function()
        add_notification(UI.Window, "Info", "Quick execute")
    end)
    add_tab("SunLarge", "SunLarge", function()
        add_notification(UI.Window, "Info", "Theme: large sun")
    end)
    add_tab("SunSmall", "SunSmall", function()
        add_notification(UI.Window, "Info", "Theme: small sun")
    end)
    add_tab("Close", "Close", function()
        UI.Visible = false
    end)
end

-- ============================================================
-- Initialize the UI when the executor loads.
-- Called from executor_main.mm after the Lua state is set up.
-- ============================================================

local function init()
    build_ui()
    build_tabs()
    add_notification(UI.Window, "Success", "Delta Executor " ..
        identifyexecutor())
end

-- Run initialization
init()

-- Return the UI module so the executor can access it.
return {
    UI = UI,
    Assets = Assets,
    Fonts = Fonts,
    init = init,
    build_ui = build_ui,
    build_tabs = build_tabs,
}
