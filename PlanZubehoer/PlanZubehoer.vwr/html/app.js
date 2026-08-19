"use strict";

/*
Name des Frontends:
Plan Zubehoer Web-Palette

Version:
0.20.0

Was macht dieses Script?
- Empfaengt die Zubehoerressourcen des aktiven Vectorworks-Dokuments.
- Erkennt Plan-* Tags automatisch.
- Ermittelt lesbare Plannamen aus der Zubehoer-Ordnerhierarchie.
- Filtert nach Plan, Zubehoertyp und Suchtext ohne erneuten SDK-Aufruf.
- Erlaubt die direkte Nutzung geeigneter Ressourcen per Doppelklick oder Button.

Was ist zu beachten?
- Direkte Aktionen sind bewusst nur fuer sichere, eindeutig behandelbare Ressourcen aktiv.
- Symbole werden zum Einsetzen aktiviert.
- Schraffuren/Bildfuellungen/Farbverlaeufe/Mosaike werden als aktuelle Fuellung gesetzt.
- Linienarten werden als aktuelle Linienart gesetzt.
- Objektstile und andere Typen werden angezeigt, aber nicht automatisch angewendet.

Welche Parameter koennen geaendert werden?
- PLAN_TAG_PREFIX: Praefix fuer Plan-Tags.
- KNOWN_PLAN_NAMES: feste Anzeigenamen, falls keine Ordnerableitung gewuenscht ist.
- STORAGE_KEY: Speichername fuer die zuletzt gewaehlten Filter.
*/

const PLAN_TAG_PREFIX = "Plan-";
const STORAGE_KEY = "STT.PlanZubehoer.v020";

const KNOWN_PLAN_NAMES = {
    "plan-08": "08 - Contractor Plan",
    "plan-cl": "Ceiling Lighting Plan",
    "plan-ml": "Millwork Lighting Plan",
    "plan-el": "Electrical Plan"
};

const TYPE_LABELS = {
    "Renderworks-Hintergruende": "Renderworks-Hintergr\u00fcnde",
    "Bildfuellungen": "Bildf\u00fcllungen",
    "Farbverlaeufe": "Farbverl\u00e4ufe"
};

const ACTION_LABELS = {
    "insert-symbol": "Symbol einsetzen",
    "set-fill": "Als aktuelle F\u00fcllung setzen",
    "set-line-type": "Als aktuelle Linienart setzen",
    "object-style": "Objektstil: keine Direktaktion",
    "unsupported": "Keine Direktaktion",
    "missing": "Ressource fehlt"
};

const state = {
    resources: [],
    plans: [],
    selectedPlan: "",
    selectedType: "Alle",
    search: "",
    selectedResourceKey: "",
    isDemo: false,
    version: "0.20.0",
    busy: false
};

const ui = {};

function norm(value) {
    return String(value ?? "").trim().toLocaleLowerCase("de-CH");
}

function escapeRegExp(value) {
    return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function isPlanTag(tag) {
    return norm(tag).startsWith(norm(PLAN_TAG_PREFIX));
}

function resourceHasPlan(resource, planTag) {
    const wanted = norm(planTag);
    return (resource.tags || []).some(tag => norm(tag) === wanted);
}

function displayType(type) {
    return TYPE_LABELS[type] || type;
}

function pathString(resource) {
    const parts = Array.isArray(resource.pathParts) ? resource.pathParts : [];
    return parts.length ? parts.join(" / ") : "oberste Ebene";
}

function resourceKey(resource) {
    return [resource.objectType || 0, resource.name || "", pathString(resource)].join("|");
}

function derivePlanName(planTag, resources) {
    const key = norm(planTag);
    if (KNOWN_PLAN_NAMES[key]) return KNOWN_PLAN_NAMES[key];

    const suffix = String(planTag).slice(PLAN_TAG_PREFIX.length).trim();
    const numeric = suffix.match(/^\d{2}$/);

    if (numeric) {
        const pattern = new RegExp("^" + escapeRegExp(suffix) + "\\s*-\\s*(.*?\\bPlan\\b)", "i");
        for (const resource of resources) {
            if (!resourceHasPlan(resource, planTag)) continue;
            for (const folder of (resource.pathParts || [])) {
                const match = String(folder).match(pattern);
                if (match) return suffix + " - " + match[1].trim();
            }
        }
    }

    return suffix || planTag;
}

function collectPlans(resources) {
    const byKey = new Map();
    for (const resource of resources) {
        for (const tag of (resource.tags || [])) {
            if (!isPlanTag(tag)) continue;
            const key = norm(tag);
            if (!byKey.has(key)) byKey.set(key, tag);
        }
    }

    return Array.from(byKey.values())
        .map(tag => ({ tag, label: derivePlanName(tag, resources) }))
        .sort((a, b) => a.label.localeCompare(b.label, "de-CH", { numeric: true, sensitivity: "base" }));
}

function collectTypes(resources, planTag) {
    const types = new Set();
    for (const resource of resources) {
        if (planTag && !resourceHasPlan(resource, planTag)) continue;
        for (const type of (resource.types || [])) types.add(type);
    }
    return Array.from(types).sort((a, b) => displayType(a).localeCompare(displayType(b), "de-CH", { sensitivity: "base" }));
}

function loadSavedState() {
    try {
        const data = JSON.parse(localStorage.getItem(STORAGE_KEY) || "{}");
        state.selectedPlan = String(data.selectedPlan || "");
        state.selectedType = String(data.selectedType || "Alle");
        state.search = String(data.search || "");
    }
    catch (err) {
        console.warn("Filterzustand konnte nicht geladen werden", err);
    }
}

function saveState() {
    try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify({
            selectedPlan: state.selectedPlan,
            selectedType: state.selectedType,
            search: state.search
        }));
    }
    catch (err) {
        console.warn("Filterzustand konnte nicht gespeichert werden", err);
    }
}

function ensureValidSelections() {
    const availablePlanKeys = new Set(state.plans.map(plan => norm(plan.tag)));
    if (!state.selectedPlan || !availablePlanKeys.has(norm(state.selectedPlan))) {
        const preferred = state.plans.find(plan => norm(plan.tag) === "plan-08");
        state.selectedPlan = preferred ? preferred.tag : (state.plans[0]?.tag || "");
    }

    const availableTypes = new Set(collectTypes(state.resources, state.selectedPlan));
    if (state.selectedType !== "Alle" && !availableTypes.has(state.selectedType)) state.selectedType = "Alle";
}

function rebuildPlanSelect() {
    ui.planSelect.innerHTML = "";
    if (!state.plans.length) {
        const option = document.createElement("option");
        option.value = "";
        option.textContent = "Keine Plan-Tags gefunden";
        ui.planSelect.append(option);
        ui.planSelect.disabled = true;
        return;
    }

    ui.planSelect.disabled = false;
    for (const plan of state.plans) {
        const option = document.createElement("option");
        option.value = plan.tag;
        option.textContent = plan.label;
        ui.planSelect.append(option);
    }
    ui.planSelect.value = state.selectedPlan;
}

function rebuildTypeSelect() {
    const types = collectTypes(state.resources, state.selectedPlan);
    ui.typeSelect.innerHTML = "";

    const all = document.createElement("option");
    all.value = "Alle";
    all.textContent = "Alle";
    ui.typeSelect.append(all);

    for (const type of types) {
        const option = document.createElement("option");
        option.value = type;
        option.textContent = displayType(type);
        ui.typeSelect.append(option);
    }
    ui.typeSelect.value = state.selectedType;
}

function filteredResources() {
    const q = norm(state.search);
    return state.resources
        .filter(resource => resourceHasPlan(resource, state.selectedPlan))
        .filter(resource => state.selectedType === "Alle" || (resource.types || []).includes(state.selectedType))
        .filter(resource => {
            if (!q) return true;
            const haystack = [
                resource.name,
                pathString(resource),
                ...(resource.tags || []),
                ...(resource.types || []).map(displayType)
            ].join(" ");
            return norm(haystack).includes(q);
        })
        .sort((a, b) => {
            const typeA = displayType((a.types || [""])[0] || "");
            const typeB = displayType((b.types || [""])[0] || "");
            const byType = typeA.localeCompare(typeB, "de-CH", { sensitivity: "base" });
            if (byType !== 0) return byType;
            return String(a.name).localeCompare(String(b.name), "de-CH", { numeric: true, sensitivity: "base" });
        });
}

function selectedResource() {
    return state.resources.find(resource => resourceKey(resource) === state.selectedResourceKey) || null;
}

function setStatus(message, kind = "") {
    ui.actionStatus.textContent = message || "";
    ui.actionStatus.dataset.kind = kind;
}

function updateActionBar() {
    const resource = selectedResource();
    if (!resource) {
        ui.useButton.disabled = true;
        ui.useButton.textContent = "Verwenden";
        ui.selectionLabel.textContent = "Keine Ressource ausgew\u00e4hlt";
        return;
    }

    const enabled = Boolean(resource.actionEnabled) && !state.busy && !state.isDemo;
    ui.useButton.disabled = !enabled;
    ui.useButton.textContent = resource.actionEnabled ? "Verwenden" : "Nicht direkt verwendbar";
    ui.selectionLabel.textContent = resource.name + " — " + (ACTION_LABELS[resource.actionCode] || "Keine Direktaktion");
}

function selectResource(resource) {
    state.selectedResourceKey = resource ? resourceKey(resource) : "";
    updateActionBar();

    for (const row of ui.resultBody.querySelectorAll("tr[data-resource-key]")) {
        row.classList.toggle("selected", row.dataset.resourceKey === state.selectedResourceKey);
    }
}

function renderTable() {
    const resources = filteredResources();
    ui.resultBody.innerHTML = "";

    if (!resources.some(resource => resourceKey(resource) === state.selectedResourceKey)) {
        state.selectedResourceKey = "";
    }

    const fragment = document.createDocumentFragment();

    for (const resource of resources) {
        const row = document.createElement("tr");
        row.dataset.resourceKey = resourceKey(resource);
        row.tabIndex = 0;
        row.title = resource.actionEnabled
            ? "Doppelklick: " + (ACTION_LABELS[resource.actionCode] || "Verwenden")
            : (ACTION_LABELS[resource.actionCode] || "Keine Direktaktion");

        const typeCell = document.createElement("td");
        typeCell.className = "type-cell";
        typeCell.textContent = (resource.types || []).map(displayType).join(", ");

        const nameCell = document.createElement("td");
        nameCell.className = "name-cell";

        const name = document.createElement("div");
        name.className = "resource-name";
        name.textContent = resource.name || "<Zubeh\u00f6r ohne Namen>";
        nameCell.append(name);

        const badges = document.createElement("div");
        badges.className = "badges";

        const tagBadge = document.createElement("span");
        tagBadge.className = "plan-tag";
        tagBadge.textContent = state.selectedPlan;
        badges.append(tagBadge);

        if (resource.actionEnabled) {
            const actionBadge = document.createElement("span");
            actionBadge.className = "action-tag";
            actionBadge.textContent = ACTION_LABELS[resource.actionCode] || "Verwenden";
            badges.append(actionBadge);
        }

        nameCell.append(badges);

        const pathCell = document.createElement("td");
        pathCell.className = "path-cell";
        pathCell.textContent = pathString(resource);

        row.append(typeCell, nameCell, pathCell);
        row.addEventListener("click", () => selectResource(resource));
        row.addEventListener("dblclick", () => useResource(resource));
        row.addEventListener("keydown", event => {
            if (event.key === "Enter") {
                event.preventDefault();
                useResource(resource);
            }
        });

        if (row.dataset.resourceKey === state.selectedResourceKey) row.classList.add("selected");
        fragment.append(row);
    }

    ui.resultBody.append(fragment);
    ui.emptyState.hidden = resources.length !== 0;
    ui.resultCount.textContent = resources.length + " Treffer";

    const activePlan = state.plans.find(plan => norm(plan.tag) === norm(state.selectedPlan));
    ui.activePlanLabel.textContent = activePlan ? activePlan.label : "";
    updateActionBar();
}

function render() {
    ensureValidSelections();
    rebuildPlanSelect();
    rebuildTypeSelect();
    ui.searchInput.value = state.search;
    renderTable();
    saveState();
}

async function useResource(resource) {
    if (!resource) resource = selectedResource();
    if (!resource) return;

    selectResource(resource);

    if (!resource.actionEnabled) {
        setStatus(ACTION_LABELS[resource.actionCode] || "F\u00fcr diesen Zubeh\u00f6rtyp gibt es keine sichere Direktaktion.", "info");
        return;
    }

    if (state.isDemo || typeof vwAPI === "undefined" || typeof vwAPI.useResource !== "function") {
        setStatus("Direktaktion ist nur innerhalb von Vectorworks verf\u00fcgbar.", "error");
        return;
    }

    if (state.busy) return;
    state.busy = true;
    updateActionBar();
    setStatus("Aktiviere „" + resource.name + "“ ...", "busy");

    try {
        const result = await vwAPI.useResource({
            name: resource.name,
            objectType: Number(resource.objectType || 0),
            actionCode: resource.actionCode
        });

        if (result && result.success) {
            const successText = resource.actionCode === "insert-symbol"
                ? "Symbol ist aktiv und kann jetzt im Plan eingesetzt werden."
                : resource.actionCode === "set-fill"
                    ? "F\u00fcllung ist jetzt als aktuelle Vorgabe gesetzt."
                    : "Linienart ist jetzt als aktuelle Vorgabe gesetzt.";
            setStatus(successText, "success");
        }
        else {
            const code = result?.code || "action-failed";
            const messages = {
                "resource-not-found": "Ressource wurde im aktiven Dokument nicht mehr gefunden. Bitte aktualisieren.",
                "object-style": "Objektstile werden nicht wie normale Symbole eingesetzt.",
                "unsupported": "F\u00fcr diesen Zubeh\u00f6rtyp gibt es keine sichere Direktaktion.",
                "action-failed": "Vectorworks konnte die Ressource nicht aktivieren."
            };
            setStatus(messages[code] || "Aktion konnte nicht ausgef\u00fchrt werden.", "error");
        }
    }
    catch (err) {
        console.error(err);
        setStatus("Fehler beim Aktivieren der Ressource.", "error");
    }
    finally {
        state.busy = false;
        updateActionBar();
    }
}

function demoSnapshot() {
    return {
        version: "0.20.0",
        source: "demo",
        count: 5,
        resources: [
            { name: "Boden - Filler 17 mm [11/16\"]", types: ["Boden-/Deckenstile"], tags: ["Plan-08"], pathParts: ["08 - Contractor Plan - Boden / Deckenstile", "08.1 - Boden"], objectType: 0, actionCode: "unsupported", actionEnabled: false },
            { name: "M-01", types: ["Schraffuren"], tags: ["Plan-08", "Plan-23"], pathParts: ["08 - Contractor Plan - Schraffuren"], objectType: 66, actionCode: "set-fill", actionEnabled: true },
            { name: "Contractor Hidden", types: ["Linienarten"], tags: ["Plan-08"], pathParts: ["08 - Contractor Plan - Linienarten"], objectType: 96, actionCode: "set-line-type", actionEnabled: true },
            { name: "Duplex Outlet", types: ["Symbole/Objektstile"], tags: ["Plan-09"], pathParts: ["09 - Power & DATA Plan - Symbole"], objectType: 16, actionCode: "insert-symbol", actionEnabled: true },
            { name: "Millwork Style", types: ["Symbole/Objektstile"], tags: ["Plan-23"], pathParts: ["23 - Millwork Plan - Symbole"], objectType: 16, actionCode: "object-style", actionEnabled: false }
        ]
    };
}

async function fetchSnapshot() {
    ui.refreshButton.disabled = true;
    ui.statusLabel.textContent = "Lese Zubeh\u00f6r...";

    try {
        let snapshot;
        if (typeof vwAPI !== "undefined" && typeof vwAPI.getSnapshot === "function") {
            snapshot = await vwAPI.getSnapshot();
            state.isDemo = false;
        }
        else {
            snapshot = demoSnapshot();
            state.isDemo = true;
        }

        state.resources = Array.isArray(snapshot.resources) ? snapshot.resources : [];
        state.version = String(snapshot.version || "0.20.0");
        state.plans = collectPlans(state.resources);
        state.selectedResourceKey = "";

        ui.versionLabel.textContent = "v" + state.version;
        ui.sourceLabel.textContent = state.isDemo ? "Vorschau ausserhalb Vectorworks" : "Aktives Dokument";
        ui.statusLabel.textContent = state.resources.length + " Zubeh\u00f6rressourcen gelesen";
        setStatus("Doppelklick auf eine aktive Ressource oder Ressource markieren und „Verwenden“ klicken.", "info");

        render();
    }
    catch (err) {
        console.error(err);
        ui.statusLabel.textContent = "Fehler beim Lesen";
        ui.emptyState.hidden = false;
        ui.emptyState.textContent = "Zubeh\u00f6rdaten konnten nicht aus Vectorworks gelesen werden.";
    }
    finally {
        ui.refreshButton.disabled = false;
    }
}

function bindEvents() {
    ui.planSelect.addEventListener("change", () => {
        state.selectedPlan = ui.planSelect.value;
        state.selectedType = "Alle";
        state.selectedResourceKey = "";
        render();
    });

    ui.typeSelect.addEventListener("change", () => {
        state.selectedType = ui.typeSelect.value;
        state.selectedResourceKey = "";
        renderTable();
        saveState();
    });

    ui.searchInput.addEventListener("input", () => {
        state.search = ui.searchInput.value;
        state.selectedResourceKey = "";
        renderTable();
        saveState();
    });

    ui.refreshButton.addEventListener("click", fetchSnapshot);
    ui.useButton.addEventListener("click", () => useResource());
}

function setupTheme() {
    if (typeof window.setVectorworksThemeCallback === "function") {
        window.setVectorworksThemeCallback((isDark) => {
            document.body.setAttribute("data-color-scheme", isDark ? "dark" : "light");
        });
    }
}

function init() {
    ui.planSelect = document.getElementById("planSelect");
    ui.typeSelect = document.getElementById("typeSelect");
    ui.searchInput = document.getElementById("searchInput");
    ui.refreshButton = document.getElementById("refreshButton");
    ui.resultCount = document.getElementById("resultCount");
    ui.activePlanLabel = document.getElementById("activePlanLabel");
    ui.resultBody = document.getElementById("resultBody");
    ui.emptyState = document.getElementById("emptyState");
    ui.versionLabel = document.getElementById("versionLabel");
    ui.statusLabel = document.getElementById("statusLabel");
    ui.sourceLabel = document.getElementById("sourceLabel");
    ui.useButton = document.getElementById("useButton");
    ui.selectionLabel = document.getElementById("selectionLabel");
    ui.actionStatus = document.getElementById("actionStatus");

    loadSavedState();
    setupTheme();
    bindEvents();
    fetchSnapshot();
}

document.addEventListener("DOMContentLoaded", init);
