class IntervalFetch {
    constructor (resource, options, doneCallback) {
        this.abortTimer = null;
        this.retryTimer = null;
        this.resource = resource;
        this.options = options;
        this.doneCallback = doneCallback;
        this.running = false;
    }

    async onTimeout() {
        const _this = this;
        this.retryTimer = null;

        // Do the fetch
        try {
            const controller = new AbortController();
            this.abortTimer = setTimeout(function() { controller.abort(); }, this.timeout );
            const response = await fetch(this.resource, { ...this.options, signal: controller.signal });

            clearTimeout(this.abortTimer);
            this.abortTimer = null;

            if (response.ok) {
                var next;
                if (this.options.useText) {
                    next = await response.text();
                } else {
                    next = await response.json();
                }
                this.doneCallback(next);
                if (this.running) {
                    this.retryTimer = setTimeout((async () => _this.onTimeout()), this.interval);
                }
            } else {
                // response not ok so try again after timeout
                if (this.running) {
                    this.retryTimer = setTimeout((async () => _this.onTimeout()), this.timeout);
                }
            }
        }
        catch (error) {
            if (error.name === "AbortError") { // timeout on fetch
                console.log("fetch timeout");
                this.abortTimer = null;
                if (this.running) {
                    this.retryTimer = setTimeout((async () => _this.onTimeout()), 0);   // fetch again immediately
                }
            } else {
                console.error(error);
                if (this.running) {
                    this.retryTimer = setTimeout((async () => _this.onTimeout()), this.timeout);  // failure try again after timeout
                }
            }
        }
    }

    start(interval = 1000, timeout = 4000) {
        var _this = this;
        this.timeout = timeout;
        this.interval = interval;
        this.retryTimer = setTimeout((async () => _this.onTimeout()), 0);
        this.running = true;
    }

    stop() {
        if (this.abortTimer) {
            clearTimeout(this.abortTimer);
            this.abortTimer = null;
        }
        if (this.retryTimer) {
            clearTimeout(this.retryTimer);
            this.retryTimer = null;
        }
        this.running = false;
    }
}

export default IntervalFetch;
