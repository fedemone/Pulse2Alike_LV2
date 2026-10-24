/*
 * Pulse2Alike - the panel's pages on a MOD device. Docs/code-notes/p2Lv2.md §16.
 *
 * A row button steps through its row's pages (OSC 1, 2, 3...), as the instrument's do; Mod and Global through
 * theirs, and the arrows through all of them. Every page's knobs and LCD values are in the template already, bound
 * to their ports by MOD; this only shows one page's and lights its LED and labels. Plain ES5: it runs in whatever
 * browser opens the device's page.
 */
function (event, funcs) {
    var icon = event.icon;
    var data = event.data;

    function pagesOf(element) {
        return ($(element).attr('data-p2a-cycle') || $(element).attr('data-p2a-page') || '').split(' ');
    }

    function show(page) {
        data.page = page;

        icon.find('.p2a-paged').each(function () {
            $(this).toggleClass('p2a-shown', $(this).attr('data-p2a-page') === page);
        });
        icon.find('.p2a-led').each(function () {
            $(this).toggleClass('p2a-lit', $(this).attr('data-p2a-page') === page);
        });
        icon.find('.p2a-box, .p2a-cell span, .p2a-shift, .p2a-button').each(function () {
            $(this).toggleClass('p2a-on', pagesOf(this).indexOf(page) >= 0);
        });
        icon.find('.p2a-face').toggleClass('p2a-off-matrix', icon.find('.p2a-cell span.p2a-on').length === 0);
    }

    // a press here is for the panel, not a drag of the pedalboard behind it
    function keep(e) {
        e.stopPropagation();
    }

    if (event.type !== 'start') {
        return;
    }

    data.order = [];
    icon.find('.p2a-lcd-title').each(function () {
        data.order.push($(this).attr('data-p2a-page'));
    });

    icon.find('[data-p2a-cycle]').on('mousedown touchstart', keep).on('click', function (e) {
        var pages = pagesOf(this);

        e.stopPropagation();
        show(pages[(pages.indexOf(data.page) + 1) % pages.length]);   // the first, from any other row's page
    });

    icon.find('[data-p2a-step]').on('mousedown touchstart', keep).on('click', function (e) {
        var n = data.order.length;

        e.stopPropagation();
        show(data.order[(data.order.indexOf(data.page) + parseInt($(this).attr('data-p2a-step'), 10) + n) % n]);
    });

    show(data.page || icon.find('.p2a-lcd-title.p2a-shown').attr('data-p2a-page') || data.order[0]);
}
