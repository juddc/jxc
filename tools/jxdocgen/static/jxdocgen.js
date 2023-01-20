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
        document.getElementById('code-style-select').value = newStyle.name;
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


document.addEventListener('DOMContentLoaded', () => {
    // phone-sized screen table of contents expando
    document.getElementById('toc-small-expand').addEventListener('click', (evt) => {
        document.getElementById('toc-small-menu').classList.toggle('expand');
    });

    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (evt) => {
        //let newColorScheme = evt.matches ? "dark" : "light";
        let localStyle = window.localStorage.getItem('code-style');
        if (localStyle === null || localStyle === 'auto') {
            setCodeStyle('auto', false);
        }
    });

    let localStyle = window.localStorage.getItem('code-style');
    if (localStyle === null) {
        localStyle = 'auto';
    }
    setCodeStyle(localStyle, true);

    // code style switcher
    let codeStyleSelector = document.getElementById('code-style-select');
    codeStyleSelector.addEventListener('change', (evt) => {
        setCodeStyle(evt.target.value, false);
    });

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
