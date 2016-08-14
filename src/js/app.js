/**
    Pebble Callbacks
**/
Pebble.addEventListener('ready', function() {
    Pebble.sendAppMessage({'AppKeyJSReady': 1}, function () {
        get_top_news();
    },
    function() {
        console.log("Failed send AppKeyJSReady");
    });
});


/**
    Utility functions
**/
function send_next_item(base_key, items, index) {
    // Build message
    var dict = {};
    dict[base_key] = items[index];

    // Send the message
    Pebble.sendAppMessage(dict, function() {
        // Use success callback to increment index
        index++;

        if(index < items.length) {
            // Send next item
            send_next_item(base_key, items, index);
        }
    },
    function() {
        console.log('Item transmission failed at index: ' + index);
        console.log(items[index]);
    });
}

function send_raw_next_item(items, index) {
    var dict = items[index]
    console.log('item: '+dict['AppKeyItemID']+' :'+dict['AppKeyItemTitle'])

    // Send the message
    Pebble.sendAppMessage(dict, function() {
        index++;
        if (index < items.length) {
            send_raw_next_item(items, index);
        }
    },
    function () {
        console.log('Failed sending raw item: ' + dict['AppKeyItemID']);
    });
}

function send_list(base_key, items) {
    var index = 0;
    send_next_item(base_key, items, index);
}

function send_raw_list(items) {
    var index = 0;
    send_raw_next_item(items, index)
}

function send_article_list(id_list, news_items) {
    // order the items
    var ordered_id = []
    var ordered_title = []

    var i;
    for (i=0; i<id_list.length; i++) {
        var id = id_list[i];
        var item = news_items[id];

        ordered_id[i] = id;
        ordered_title[i] = item.title.substring(0, 63)
    }

    // send the number of items, then send list
    Pebble.sendAppMessage({'AppKeyNumItems': id_list.length}, function() {
        send_list('AppKeyItemID', ordered_id);
        send_list('AppKeyItemTitle', ordered_title)

        console.log('Sent all data');
    },
    function() {
        console.log('Failed to send the number of items');
    });
}

/**
    Web requests
**/
var id_list = []
var news_items = [];
var news_limit = 10;
var news_count = 0;

function get_top_news() {
    // build REST request
    var request = new XMLHttpRequest();
    request.onload = function() {
        // acquire the list of top news items
        id_list = JSON.parse(this.responseText);
        // id_list.sort(function(a, b){ return a-b})

        console.log(id_list);

        var i;
        for (i=0; i<id_list.length; i++) {
            get_article_info(id_list[i]);
        }
    };

    var url = 'https://hacker-news.firebaseio.com/v0/topstories.json?print=pretty&orderBy="$key"&&limitToFirst='+news_limit;
    request.open('GET', url);
    request.send();
}

function get_article_info(id) {
    var request = new XMLHttpRequest();
    request.onload = function() {
        var item = JSON.parse(this.responseText);
        news_items[item.id] = item;
        news_count++;

        // received last details, send on list
        if (news_count == news_limit) {
            send_article_list(id_list, news_items);
        }
    };

    url = 'https://hacker-news.firebaseio.com/v0/item/'+id+'.json';
    request.open('GET', url)
    request.send();
}
