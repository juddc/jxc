function getDiagram(groupRefName) {
    if (groupRefName.toLocaleLowerCase() === 'ws') {
        groupRefName = 'whitespace';
    } else if (groupRefName.toLocaleLowerCase() == 'ws?') {
        groupRefName = 'whitespace_optional';
    }

    if (!(groupRefName in window.DIAGRAMS)) {
        console.error(`Unknown MatchGroupRef "${groupRefName}"`);
        return null;
    }

    return window.DIAGRAMS[groupRefName];
}

function getDiagramName(diagNode) {
    return diagNode.hasAttribute('data-diagram-name') ? diagNode.getAttribute('data-diagram-name') : '';
}

function findAllDiagrams() {
    let result = {};
    for (let node of document.querySelectorAll('.diagram')) {
        if (node && node.hasAttribute("data-diagram-name")) {
            result[node.getAttribute('data-diagram-name')] = node;
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


/*
class Tooltip {
    constructor(nodeID) {
        this.visible = false;
        this.currentMouseX = 0;
        this.currentMouseY = 0;
        this.node = null;
        this.nodeID = nodeID;
        this.offsetY = 30;
        this.fadeTimeMS = 150;

        this._fadeOutTimeout = null;
    }

    onDOMReady() {
        this.node = document.getElementById(this.nodeID);
        if (!this.node) {
            console.error(`No tooltip in page with id '${nodeID}'`);
        }

        //this.node.style.transitionDelay = `${this.fadeTimeMS}ms`;
        this.node.style.animationDuration = `${this.fadeTimeMS}ms`;
    }

    onMouseMove(x, y) {
        this.currentMouseX = x;
        this.currentMouseY = y;
        if (this.node && this.visible) {
            let tooltipRect = this.node.getBoundingClientRect();
            let centerX = tooltipRect.width / 2;

            let newLeft = `${x - centerX}px`;
            let newTop = `${y + this.offsetY}px`;
            if (this.node.style.left !== newLeft) {
                this.node.style.left = newLeft;
            }
            if (this.node.style.top !== newTop) {
                this.node.style.top = newTop;
            }
        }
    }

    _hasAttr(name) {
        return this.node && this.node.hasAttribute(name);
    }

    _getAttr(name, convertFunc = null) {
        if (convertFunc) {
            return (this.node && this.node.hasAttribute(name)) ? convertFunc(this.node.getAttribute(name)) : null;
        }
        return this.node ? this.node.getAttribute(name) : null;
    }

    _setAttr(name, value) {
        if (this.node) {
            this.node.setAttribute(name, `${value}`);
        }
    }

    setVisible(newName, contentSelector) {
        if (!this.node) {
            return;
        }

        // use the name attribute to determine if we need to change the _contents_ of the tooltip
        if (this._getAttr('data-name') !== newName) {
            this._setAttr('data-name', newName);

            // update tooltip content - get a document element from contentSelector
            let contentEle = null;
            if (typeof contentSelector === 'string') {
                contentEle = document.querySelector(contentSelector);
            } else if (typeof contentSelector === 'function') {
                contentEle = contentSelector();
            } else {
                contentEle = contentSelector;
            }

            if (contentEle) {
                this.node.innerHTML = contentEle.innerHTML;
            } else {
                this.node.innerHTML = '';
            }
        }

        // fade in
        this.node.classList.remove('hide');
        this.node.classList.add('show');
        this.visible = true;
    }

    setHidden() {
        if (!this.node) {
            return;
        }

        // fade out
        this.node.classList.add('hide');
        this.node.classList.remove('show');
        this.visible = false;
    }
}
*/


window.TOOLTIP_VISIBLE = false;


function updateTooltipPosition(x, y) {
    if (!window.TOOLTIP_VISIBLE) {
        return;
    }

    let tooltip = document.getElementById('tooltip');

    let offsetY = 30;
    let tooltipRect = tooltip.getBoundingClientRect();
    let centerX = tooltipRect.width / 2;

    let newLeft = `${x - centerX}px`;
    let newTop = `${y + offsetY}px`;
    if (tooltip.style.left !== newLeft) {
        tooltip.style.left = newLeft;
    }
    if (tooltip.style.top !== newTop) {
        tooltip.style.top = newTop;
    }
}


function setTooltipContent(contentName, contentSelector) {
    let tooltip = document.getElementById('tooltip');
    let currentContentName = tooltip.getAttribute('data-name');

    if (currentContentName !== contentName) {
        tooltip.setAttribute('data-name', contentName);

        // update tooltip content - get a document element from contentSelector
        let contentEle = null;
        if (typeof contentSelector === 'string') {
            contentEle = document.querySelector(contentSelector);
        } else if (typeof contentSelector === 'function') {
            contentEle = contentSelector();
        } else {
            contentEle = contentSelector;
        }

        if (contentEle) {
            tooltip.innerHTML = contentEle.innerHTML;
        } else {
            tooltip.innerHTML = '';
        }
    }
}


function setTooltipVisible(isVisible) {
    let tooltip = document.getElementById('tooltip');
    if (isVisible) {
        tooltip.classList.remove('hide');
        tooltip.classList.add('show');
        window.TOOLTIP_VISIBLE = true;
    } else {
        tooltip.classList.add('hide');
        tooltip.classList.remove('show');
        window.TOOLTIP_VISIBLE = false;
    }
}


// Hacky way of checking if we're on a touch device.
// Without this, the diagram tooltips show up on click after scrolling down the page.
function isTouchscreenDevice() {
    return 'ontouchstart' in document.documentElement || navigator.maxTouchPoints > 0 || navigator.msMaxTouchPoints > 0;
}


document.addEventListener('DOMContentLoaded', () => {
    document.getElementsByTagName('html')[0].onmousemove = (evt) => {
        updateTooltipPosition(evt.clientX, evt.clientY);
    };

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

    // allow clicking on match groups, and add a tooltip on hover to preview them
    let groupLinks = document.querySelectorAll('.MatchGroupRef');
    for (let link of groupLinks) {
        let textEle = link.querySelector('text');

        // each text element has a rect element just before it - that rect is what we want to interact with
        let rectEle = null;
        if (textEle && textEle.previousSibling && textEle.previousSibling.nodeName === 'rect') {
            rectEle = textEle.previousSibling;
        }

        let diagNode = getDiagram(textEle.innerHTML);
        let diagName = getDiagramName(diagNode);

        if (diagName in window.DIAGRAMS) {
            rectEle.addEventListener('click', () => {
                if (diagName) {
                    window.location.hash = diagName;
                }
            });

            if (!isTouchscreenDevice()) {
                rectEle.addEventListener('mouseover', () => {
                    setTooltipContent(diagName, () => window.DIAGRAMS[diagName].querySelector('.diagram-outer'));
                    setTooltipVisible(true);
                });

                rectEle.addEventListener('mouseout', (evt) => {
                    setTooltipVisible(false);
                });
            }
        }
    }
});
