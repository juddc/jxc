function getDiagram(name) {
    return document.getElementById(`diagram-${name}`);
}

function onClickDiagramInSidebar(name) {

}

function onClickDiagramMatchGroupRef(name) {
    let found = false;
    for (let diag of window.DIAGRAMS) {
        if (diag === name) {
            found = true;
            break;
        }
    }

    if (!found) {
        console.error(`Clicked unknown MatchGroupRef "${name}"`);
        return;
    }

    window.location.hash = name;
}

function findAllDiagrams() {
    let result = [];
    for (let node of document.querySelectorAll('.diagram')) {
        if (node && node.hasAttribute("data-diagram-name")) {
            result.push(node.getAttribute('data-diagram-name'));
        }
    }
    return result;
}


function findCodeStyleIndex(styleName) {
    for (let i = 0; i < window.CODE_STYLES.length; i++) {
        if (window.CODE_STYLES[i].name === styleName) {
            return i;
        }
    }
    return null;
}


function isBrowserInDarkMode() {
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
}


function setCodeStyle(newStyleName, updateSelect = true) {
    let newStyleIndex = findCodeStyleIndex(newStyleName);
    if (newStyleIndex === null && newStyleName !== 'auto') {
        console.error(`Invalid style ${newStyleName}`);
        return;
    }

    let newStyle = null;
    if (newStyleIndex !== null) {
        newStyle = window.CODE_STYLES[newStyleIndex];
    } else {
        newStyle = isBrowserInDarkMode() ? window.DEFAULT_DARK_STYLE : window.DEFAULT_LIGHT_STYLE;
    }

    // set dropdown value
    if (updateSelect && newStyleIndex !== null) {
        for (let codeStyleSelector of document.querySelectorAll('select.code-theme')) {
            codeStyleSelector.value = newStyle.name;
        }
    }

    // update code block classes
    for (let codeBlock of document.querySelectorAll('.code')) {
        for (let style of window.CODE_STYLES) {
            codeBlock.classList.remove(style.name);
        }
        codeBlock.classList.add(newStyle.name);
    }

    // update body classes
    let bodyEle = document.getElementsByTagName('body')[0];
    bodyEle.classList.remove('light');
    bodyEle.classList.remove('dark');
    bodyEle.classList.add(newStyle.type);

    // save the style so it persists between pages
    window.localStorage.setItem('code-style', newStyleName);
}


function toggleSidebarVisible() {
    document.getElementById('sidebar').classList.toggle('expand');
    document.getElementById('body-mask').classList.toggle('expand');
}


function languageTagToLabel(lang) {
    if (typeof lang !== 'string' || lang.length == 0) {
        return `${lang}`;
    }

    switch (lang) {
        case "jxc": return "JXC";
        case "json": return "JSON";
        case "cpp": return "C++";
        case "python": return "Python";
        default: break;
    }

    let result = [];
    for (let i = 0; i < lang.length; i++) {
        result.push(lang[i]);
    }
    result[0] = result[0].toLocaleUpperCase();
    return result.join('');
}


document.addEventListener('DOMContentLoaded', () => {
    // add language labels for code language blocks
    let langPrefix = 'language-';
    for (let codeBlock of document.querySelectorAll('.code')) {
        let langClass = null;
        for (let cls of codeBlock.classList) {
            if (cls.length > langPrefix.length && cls.startsWith(langPrefix)) {
                langClass = cls;
                break;
            }
        }

        if (langClass === null) {
            continue;
        }

        let lang = langClass.substring(langPrefix.length);

        if (lang !== null) {
            let labelEle = document.createElement('span');
            labelEle.classList.add('language-label');
            labelEle.classList.add(langClass);
            labelEle.innerHTML = languageTagToLabel(lang);

            let codeClone = codeBlock.cloneNode();
            for (let child of codeBlock.childNodes) {
                codeClone.appendChild(child);
            }

            let codeWrap = document.createElement('div');
            codeWrap.classList.add('code-wrap');
            codeWrap.appendChild(labelEle);
            codeWrap.appendChild(codeClone);
            codeBlock.replaceWith(codeWrap);
        }
    }

    // set code style to the user's selected setting on first page load
    let localStyle = window.localStorage.getItem('code-style');
    if (localStyle === null) {
        localStyle = 'auto';
    }
    setCodeStyle(localStyle, true);

    // phone-sized screen table of contents expando
    for (let button of document.querySelectorAll('.sidebar-toggle')) {
        button.addEventListener('click', (evt) => {
            toggleSidebarVisible();
        });
    }

    // allow tapping the space outside the slide-in sidebar to close it (phone-sized screens only)
    document.getElementById('body-mask').addEventListener('click', (evt) => {
        toggleSidebarVisible();
    });

    // if code style is auto, automatically change it when light/dark mode changes
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (evt) => {
        //let newColorScheme = evt.matches ? "dark" : "light";
        let localStyle = window.localStorage.getItem('code-style');
        if (localStyle === null || localStyle === 'auto') {
            setCodeStyle('auto', false);
        }
    });

    // code style switchers
    for (let codeStyleSelector of document.querySelectorAll('select.code-theme')) {
        codeStyleSelector.addEventListener('change', (evt) => {
            setCodeStyle(evt.target.value, false);
        });
    }

    // diagrams
    window.DIAGRAMS = findAllDiagrams();

    let urlDiagram = window.location.hash;
    if (urlDiagram.length > 0 && urlDiagram[0] == '#') {
        urlDiagram = urlDiagram.substring(1);
    }

    // allow clicking on match groups
    let groupLinks = document.querySelectorAll('.MatchGroupRef');
    for (let link of groupLinks) {
        link.onclick = () => {
            onClickDiagramMatchGroupRef(link.querySelector('text').innerHTML);
        };
    }
});
