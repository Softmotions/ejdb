window.DOCUMENTATION_OPTIONS = {
    URL_ROOT: '',
    VERSION: '1.2.12',
    COLLAPSE_INDEX: false,
    FILE_SUFFIX: '.html',
    HAS_SOURCE: 'true' === 'true'
};

/**
 * Copyright Marc J. Schmidt. See the LICENSE file at the top-level
 * directory of this distribution and at
 * https://github.com/marcj/css-element-queries/blob/master/LICENSE.
 */
(function () {

    /**
     * Class for dimension change detection.
     *
     * @param {Element|Element[]|Elements|jQuery} element
     * @param {Function} callback
     *
     * @constructor
     */
    this.ResizeSensor = function (element, callback) {
        /**
         *
         * @constructor
         */
        function EventQueue() {
            this.q = [];
            this.add = function (ev) {
                this.q.push(ev);
            };

            var i, j;
            this.call = function () {
                for (i = 0, j = this.q.length; i < j; i++) {
                    this.q[i].call();
                }
            };
        }

        /**
         * @param {HTMLElement} element
         * @param {String}      prop
         * @returns {String|Number}
         */
        function getComputedStyle(element, prop) {
            if (element.currentStyle) {
                return element.currentStyle[prop];
            } else if (window.getComputedStyle) {
                return window.getComputedStyle(element, null).getPropertyValue(prop);
            } else {
                return element.style[prop];
            }
        }

        /**
         *
         * @param {HTMLElement} element
         * @param {Function}    resized
         */
        function attachResizeEvent(element, resized) {
            if (!element.resizedAttached) {
                element.resizedAttached = new EventQueue();
                element.resizedAttached.add(resized);
            } else if (element.resizedAttached) {
                element.resizedAttached.add(resized);
                return;
            }

            element.resizeSensor = document.createElement('div');
            element.resizeSensor.className = 'resize-sensor';
            var style = 'position: absolute; left: 0; top: 0; right: 0; bottom: 0; overflow: scroll; z-index: -1; visibility: hidden;';
            var styleChild = 'position: absolute; left: 0; top: 0;';

            element.resizeSensor.style.cssText = style;
            element.resizeSensor.innerHTML =
                '<div class="resize-sensor-expand" style="' + style + '">' +
                '<div style="' + styleChild + '"></div>' +
                '</div>' +
                '<div class="resize-sensor-shrink" style="' + style + '">' +
                '<div style="' + styleChild + ' width: 200%; height: 200%"></div>' +
                '</div>';
            element.appendChild(element.resizeSensor);

            if (!{fixed: 1, absolute: 1}[getComputedStyle(element, 'position')]) {
                element.style.position = 'relative';
            }

            var expand = element.resizeSensor.childNodes[0];
            var expandChild = expand.childNodes[0];
            var shrink = element.resizeSensor.childNodes[1];
            var shrinkChild = shrink.childNodes[0];

            var lastWidth, lastHeight;

            var reset = function () {
                expandChild.style.width = expand.offsetWidth + 10 + 'px';
                expandChild.style.height = expand.offsetHeight + 10 + 'px';
                expand.scrollLeft = expand.scrollWidth;
                expand.scrollTop = expand.scrollHeight;
                shrink.scrollLeft = shrink.scrollWidth;
                shrink.scrollTop = shrink.scrollHeight;
                lastWidth = element.offsetWidth;
                lastHeight = element.offsetHeight;
            };

            reset();

            var changed = function () {
                if (element.resizedAttached) {
                    element.resizedAttached.call();
                }
            };

            var addEvent = function (el, name, cb) {
                if (el.attachEvent) {
                    el.attachEvent('on' + name, cb);
                } else {
                    el.addEventListener(name, cb);
                }
            };

            addEvent(expand, 'scroll', function () {
                if (element.offsetWidth > lastWidth || element.offsetHeight > lastHeight) {
                    changed();
                }
                reset();
            });

            addEvent(shrink, 'scroll', function () {
                if (element.offsetWidth < lastWidth || element.offsetHeight < lastHeight) {
                    changed();
                }
                reset();
            });
        }

        if ("[object Array]" === Object.prototype.toString.call(element)
            || ('undefined' !== typeof jQuery && element instanceof jQuery) //jquery
            || ('undefined' !== typeof Elements && element instanceof Elements) //mootools
        ) {
            var i = 0, j = element.length;
            for (; i < j; i++) {
                attachResizeEvent(element[i], callback);
            }
        } else {
            attachResizeEvent(element, callback);
        }

        this.detach = function () {
            ResizeSensor.detach(element);
        };
    };

    this.ResizeSensor.detach = function (element) {
        if (element.resizeSensor) {
            element.removeChild(element.resizeSensor);
            delete element.resizeSensor;
            delete element.resizedAttached;
        }
    };
})();

/*
 * jQuery appear plugin
 *
 * Copyright (c) 2012 Andrey Sidorov
 * licensed under MIT license.
 *
 * https://github.com/morr/jquery.appear/
 *
 * Version: 0.3.4
 */
(function ($) {
    var selectors = [];

    var check_binded = false;
    var check_lock = false;
    var defaults = {
        interval: 250,
        force_process: false
    };
    var $window = $(window);

    var $prior_appeared;

    function process() {
        check_lock = false;
        for (var index = 0, selectorsLength = selectors.length; index < selectorsLength; index++) {
            var $appeared = $(selectors[index]).filter(function () {
                return $(this).is(':appeared');
            });

            $appeared.trigger('appear', [$appeared]);

            if ($prior_appeared) {
                var $disappeared = $prior_appeared.not($appeared);
                $disappeared.trigger('disappear', [$disappeared]);
            }
            $prior_appeared = $appeared;
        }
    }

    // "appeared" custom filter
    $.expr[':']['appeared'] = function (element) {
        var $element = $(element);
        if (!$element.is(':visible')) {
            return false;
        }

        var window_left = $window.scrollLeft();
        var window_top = $window.scrollTop();
        var offset = $element.offset();
        var left = offset.left;
        var top = offset.top;

        if (top + $element.height() >= window_top &&
            top - ($element.data('appear-top-offset') || 0) <= window_top + $window.height() &&
            left + $element.width() >= window_left &&
            left - ($element.data('appear-left-offset') || 0) <= window_left + $window.width()) {
            return true;
        } else {
            return false;
        }
    };

    $.fn.extend({
        // watching for element's appearance in browser viewport
        appear: function (options) {
            var opts = $.extend({}, defaults, options || {});
            var selector = this.selector || this;
            if (!check_binded) {
                var on_check = function () {
                    if (check_lock) {
                        return;
                    }
                    check_lock = true;
                    setTimeout(process, opts.interval);
                };

                $(window).scroll(on_check).resize(on_check);
                check_binded = true;
            }
            if (opts.force_process) {
                setTimeout(process, opts.interval);
            }
            selectors.push(selector);
            return $(selector);
        }
    });

    $.extend({
        // force elements's appearance check
        force_appear: function () {
            if (check_binded) {
                process();
                return true;
            }
            return false;
        }
    });
})(jQuery);


jQuery(function () {

    initCSS();
    initMaterialize();
    initTopNav();
    initSideNav();
    initRelNav();
    initBtt();
    initImgFull();
    initKeys();
    initScreen();

    $(window).on("load", function () {
        var hash = document.location.hash;
        if (hash != "") {
            setTimeout(function () {
                if (location.hash) {
                    window.location.href = hash;
                }
            }, 0);
        }
    });

    function initImgFull() {
        $(".section img").on("click", function () {
            var w = $(this).width();
            $(this).toggleClass("img-full");
            var w2 = $(this).width();
            if (w === w2) {
                $(this).removeClass("img-full");
            }
        });
    }

    function initCSS() {
        $("table.docutils").each(function () {
            var $this = $(this);
            if (!$this.hasClass("footnote") && !$this.hasClass("citation")) { //skip footnote/citation tables
                $this.addClass("bordered hoverable");
            }
        });
    }

    function initMaterialize() {
        $(".button-collapse").sideNav();
        $(".dropdown-button").dropdown(
            {
                "hover": false,
                "constrain_width": false
            });
        $("select").not(".disabled").material_select();
        $(".modal-trigger").leanModal();
    }

    function initTopNav() {
        $("a#toggle-search").click(function () {
            var el = $("div#search");
            el.is(":visible") ? el.slideUp() : el.slideDown(function () {
                                  el.find("input").focus();
                              });
            return false;
        });
    }

    function initSideNav() {
        var res = $("#sidetoc");
        var tlfirst = false;
        res.find("> ul > li").each(function () {
            var $this = $(this);
            var $children = $this.children();
            if ($children.length < 2) {
                $children.first().parent("li").addClass("mt-toclink");
                if (!tlfirst) {
                    $children.first().parent("li").addClass("mt-first");
                    tlfirst = true;
                }
                return;
            }
            tlfirst = false;
            var $aEl = $children.first();
            /*$aEl.click(function(e) {
             e.preventDefault();
             window.setTimeout(function() {
             var href = $(e.delegateTarget).attr("href");
             if (href != null) {
             window.location.href = href;
             }
             }, 300);
             });*/

            var $uEl = $children.next();
            if ($aEl.parent().hasClass("current")) {
                $aEl.addClass("active");
            }
            //$aEl.addClass("collapsible-header waves-effect waves-teal z-depth-1");
            $aEl.addClass("collapsible-header z-depth-1");
            $uEl.addClass("collapsible-body");
        });
        res = res.find("> ul");

        var sidebar = $("#sidebar");
        var pageContent = $("#page-content-block");
        if (sidebar.length) {
            pageContent.css({"min-height": sidebar.height() + "px"});
            new ResizeSensor(sidebar[0], function () {
                pageContent.css({"min-height": (window.innerWidth <= 601 ? 0 : sidebar.height()) + "px"});
            });
        } else {
            pageContent.css({"min-height": 0 + "px"});
        }
        res.collapsible();
        res.each(function () {
            var $this = $(this);
            $this.off("click.collapse", ".collapsible-header");
            $this.find("> li > .collapsible-header").off("click.collapse");
        });
    }

    function initRelNav() {
        var topRel = $("#page-content-block").find("> .rel-nav");
        var bottomRel = $("#bottom-rel-placeholder");
        $(document).ready(function () {
            $.force_appear();
        });
        topRel.appear({
            interval: 100,
            force_process: true
        });
        bottomRel.show();
        topRel.on("disappear", function () {
            bottomRel.show();
        });
        topRel.on("appear", function () {
            bottomRel.hide();
        });
    }


    function initBtt() {
        /*
         Back to top button.
         Based on http://codyhouse.co/gem/back-to-top
         */
        $(document).ready(function ($) {
            // browser window scroll (in pixels) after which the "back to top" link is shown
            var offset = 300;
            //browser window scroll (in pixels) after which the "back to top" link opacity is reduced
            var offset_opacity = 1200;
            //duration of the top scrolling animation (in ms)
            var scroll_top_duration = 700;
            //grab the "back to top" link
            var $btt = $("#btt");

            //hide or show the "back to top" link
            $(window).scroll(function () {
                ($(this).scrollTop() > offset) ?
                $btt.addClass("btt-visible") : $btt.removeClass("btt-visible btt-fade-out");
                if ($(this).scrollTop() > offset_opacity) {
                    $btt.addClass("btt-fade-out");
                }
            });

            //smooth scroll to top
            $btt.on("click", function (event) {
                event.preventDefault();
                $("body,html").animate({scrollTop: 0}, scroll_top_duration);
            });
        });
    }

    function initKeys() {
        $(document).keydown(function (e) {
            e = e || window.event;
            var cc = e.which || e.keyCode;
            var ctrl = e.altKey || e.ctrlKey || e.shiftKey;
            var href;
            if (cc === 39 && !ctrl) { // right
                href = $("div.rel-nav > a > i.mts-right").first().parent("a").attr("href");
            } else if (cc === 37 && !ctrl) { // left
                if (window.history && window.history.length) {
                    window.history.back();
                } else {
                    href = $("div.rel-nav > a > i.mts-left").first().parent("a").attr("href");
                }
            } else if (cc === 38 && e.ctrlKey) { // up
                href = $("#relnav-parents-1").find("> li > a").last().attr("href");
            } else if (cc === 36) { // home
                href = $("div.rel-nav > a > i.mdi-action-home").first().parent("a").attr("href");
            }
            if (href) {
                window.location.href = href;
            }
        });
    }

    function initScreen() {
        $(document).ready(setPos);
        $(window).load(setPos);
        function setPos() {
            var hash = document.location.hash;
            if (hash != "") {
                return;
            }
            var width = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
            if (width < 601) {
                var el = $("#page-content-block");
                if (el.length) {
                    $(window).scrollTop(el.offset().top);
                }
            }
        }
    }
});



