import sys, os, json
from playwright.sync_api import sync_playwright
js = open('rbfont.js').read()
with sync_playwright() as p:
    b = p.chromium.launch()
    for name in sys.argv[1:]:
        svg = open(name + '.svg').read()
        html = f"""<!doctype html><html><head><meta charset="utf-8"><style>html,body{{margin:0;background:#121212}}svg{{display:block}}</style></head>
<body>{svg}<script>{js}
const wm = document.getElementById('wm');
const big = (t, col, x) => {{ const a = rbPaths(t, 23, 1.7, 1.2); return {{ a, svg: `<g transform="translate(${{x}},0)"><g fill="none" stroke="${{col}}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">${{a.paths}}</g><g fill="none" stroke="#0c0c0c" stroke-width=".32" stroke-linecap="round" stroke-linejoin="round">${{a.paths}}</g></g>` }}; }};
const p1 = big('SPY', '#f4f1e8', 0), p2 = big('BAND', '#f4f1e8', p1.a.width + 10);
wm.innerHTML = p1.svg + p2.svg;
window.wmWidth = p1.a.width + 10 + p2.a.width;
rbConvert();
</script></body></html>"""
        open(name + '.html', 'w').write(html)
        pg = b.new_page(viewport={'width': 1200, 'height': 690}, device_scale_factor=2)
        pg.goto('file://' + os.path.abspath(name + '.html')); pg.wait_for_timeout(300)
        print(name, 'wordmark', pg.evaluate('window.wmWidth'), 'page', pg.evaluate('[document.body.scrollWidth, document.body.scrollHeight]'))
        pg.screenshot(path=name + '-clean.png', clip={'x': 0, 'y': 0, 'width': 1200, 'height': 690})
    b.close()
