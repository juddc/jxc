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


document.addEventListener('DOMContentLoaded', () => {
    // code style switcher
    let styleSwitcherButton = document.getElementById('code-style-switcher');
    styleSwitcherButton.classList.add(window.CODE_STYLES[0].type);

    styleSwitcherButton.onclick = (evt) => {
        console.assert(window.CODE_STYLES.length > 0);
        window.CODE_STYLE_INDEX += 1;
        if (window.CODE_STYLE_INDEX >= window.CODE_STYLES.length) {
            window.CODE_STYLE_INDEX = 0;
        }

        let newStyleName = window.CODE_STYLES[window.CODE_STYLE_INDEX].name;
        let newStyleType = window.CODE_STYLES[window.CODE_STYLE_INDEX].type;

        for (let codeBlock of document.querySelectorAll('.code')) {
            for (let style of window.CODE_STYLES) {
                codeBlock.classList.remove(style.name);
            }
            codeBlock.classList.add(newStyleName);
        }

        styleSwitcherButton.classList.remove('light');
        styleSwitcherButton.classList.remove('dark');
        styleSwitcherButton.classList.add(newStyleType);
    };

    // diagrams
    window.DIAGRAMS = findAllDiagrams();

    let urlDiagram = window.location.hash;
    if (urlDiagram.length > 0 && urlDiagram[0] == '#') {
        urlDiagram = urlDiagram.substr(1);
    }

    let groupLinks = document.querySelectorAll('.MatchGroupRef');
    for (let link of groupLinks) {
        link.onclick = () => {
            onClickDiagramMatchGroupRef(link.querySelector('text').innerHTML);
        };
    }
});
