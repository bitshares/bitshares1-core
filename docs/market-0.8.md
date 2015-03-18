
Fixes to v0.8 market engine
----------------------------

This file describes the market engine cleanup commits for 0.8.x.

[638edfcec8a7505580055a06dcf98de2b240211a](https://github.com/BitShares/bitshares/commit/638edfcec8a7505580055a06dcf98de2b240211a) market_engine.cpp: Refactor get_next_ask() body into three separate functions

This commit simply reflows the three parts of `get_next_ask()` body into three sub-functions.  At various points, the old version of `get_next_ask()` will return `true`.  This return is passed through from the sub-function to the caller of `get_next_ask()`.  The end of each sub-function returns `false`, and in this case `get_next_ask` will advance to the next sub-function.

[6aac392a860cf99f34ef6c4cc5a0801711c6844a](https://github.com/BitShares/bitshares/commit/6aac392a860cf99f34ef6c4cc5a0801711c6844a) market_engine.cpp: Simplify unnecessary valid() checks

This commit reflows an `if` statement and a `switch` statement (the latter was left over from relative orders implementation).  In addition there are two places where the optional field `_current_ask` is assigned a (non-optional) value, `_current_ask.valid()` must return `true` so we don't even need to bother calling it.  Again no semantics changes here.

[149bf83e24e9f8296e309bc2aa678a983c57833a](https://github.com/BitShares/bitshares/commit/149bf83e24e9f8296e309bc2aa678a983c57833a) market_engine.cpp: Simplify mtrx.bid_price computation for shorts

`get_next_bid()` doesn't return shorts if there is no feed.  Thus if it returned a short, we simply `FC_ASSERT` the feed price is valid (this check should never fail).  Then instead of unconditionally assigning `mtrx.bid_price` above and re-assigning it in the `if` statement, we instead assign it along both paths of the `if` statement.

[a165663964bab4cc0fe94c5fd50b4747b0462214](https://github.com/BitShares/bitshares/commit/a165663964bab4cc0fe94c5fd50b4747b0462214) market_engine.cpp: Use three passes to implement margin calls, expiration, and ask orders

This is the first semantics change.  The `get_next_ask()` function will only call a single subfunction, but the market engine's main loop will re-execute.  Match as far as you can using margin calls; then match as far as you can using expired margin orders; then match as far as you can using regular ask orders.  This properly implements the intended prioritization semantics.

[c07cf63f176996b83a696fd9fe544d0beb94f83a](https://github.com/BitShares/bitshares/commit/c07cf63f176996b83a696fd9fe544d0beb94f83a) market_engine: Reflow three-way choice between called, expired and inactive margin positions

The `minimum_ask()` function is renamed to more descriptive `minimum_cover_ask_price()`.

This commit cleans up the `_current_ask->type == cover_order` processing in the main loop, using the fact that we now have passes.  When there are no bids that match the `minimum_cover_ask_price()`, we can simply terminate the current iteration.

The rest of the logic in this block attempts to be a three-way case selection between margin calls, expired covers and inactive covers.  The previous logic is confusing and error-riddled.  First of all a set of apparently missing brackets leaps out.  Second we have confusing "inclusion-exclusion" logic where we first check for an expired cover, THEN check for a called-and-expired cover.

The new code flow uses an `if / else if / else` construct to emphasize the three-way choice.  The only effect is to write the order's effective price into `mtrx.ask_price`, the actual termination of the loop is handled later (see next commit).

[d08f0d5cd32fa85b2a95b90f0017f2064b5fec2f](https://github.com/BitShares/bitshares/commit/d08f0d5cd32fa85b2a95b90f0017f2064b5fec2f) market_engine: Spread terminates current market engine pass regardless of order types

The algorithm in `get_next_bid()` returns bids in book order, i.e. if nothing matches the current bid then later bids will not match either.
Each of the subfunctions of `get_next_ask()` returns asks in book order, i.e. if nothing matches the current ask then later asks *in the same pass* will not match either.

Combining these, it is safe to terminate the current pass when there is a spread.  I.e. if the top-of-book ask and bid cannot match, then the current pass is finished.

[a60ea16b0624b46441363af737e203eaa72304c7](https://github.com/BitShares/bitshares/commit/a60ea16b0624b46441363af737e203eaa72304c7) market_engine: Remove obsolete _short_itr

We had a `short_itr` which at one time walked through all shorts.  However we never need to walk through all shorts, instead we do two separate walks, one through shorts at limit and another through shorts at feed.  Indeed `_short_itr` is initialized but never used.  So get rid of it!

[25fbcfe7f85f6cc0d5c8a10aa2573099c2711d0d](https://github.com/BitShares/bitshares/commit/25fbcfe7f85f6cc0d5c8a10aa2573099c2711d0d) chain_database.cpp: Fix comparisons in _shorts_at_feed_index

This commit is a result of careful auditing of the reclassification logic moving shorts into and out of the `_shorts_at_feed` index.  `_shorts_at_feed` contains order `o` if and only if `o.limit_price >= feed`.

We move all comparisons so the order is on the left and the feed is on the right.  We find one place where the strictness of the inequality is wrong, and two places where the direction of the inequality is wrong.

[7d6c563e9eed203eedf4c63d17d15a12faabbb71](https://github.com/BitShares/bitshares/commit/7d6c563e9eed203eedf4c63d17d15a12faabbb71) market_engine: Initialize _short_at_limit_itr with current_pair instead of next_pair

This commit is a result of careful auditing of the iterator logic.  In particular we want to go through shorts whose limit is less than the feed price, and the iterator must start *in the current pair*.  This code was clearly copy-pasted from another iterator which starts at the next pair and goes in reverse.

[7038f30f43a54d4fd7436776f110e3da180f1b00](https://github.com/BitShares/bitshares/commit/7038f30f43a54d4fd7436776f110e3da180f1b00) market_engine: Fetch short balances from pending chain state instead of database

This commit is a result of careful auditing of directly talking to the database.  In particular one spot where short orders were pulled out of the DB, when they should be pulled out of pending chain state instead.

