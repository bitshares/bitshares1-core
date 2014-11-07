# BitShares Acceptance Testing

This repository holds the BitShares acceptance tests implemented on top of Ruby's cucumber framework.


## Installation

Install Ruby:

On Linux I recommend to use rvm to install Ruby, here is how to do this on Ubuntu:

``` bash
  $ sudo apt-get install libgdbm-dev libncurses5-dev automake libtool bison libffi-dev
  $ curl -L https://get.rvm.io | bash -s stable
  $ source ~/.rvm/scripts/rvm
  $ echo "source ~/.rvm/scripts/rvm" >> ~/.bashrc
  $ rvm install 2.1.3
  $ rvm use 2.1.3 --default
```


On OS X you can install it via brew:

  $ brew install ruby
  
  
After you installed Ruby, install bundler gem:

  $ gem install bundler
  

Now you can install all dependencies by typing 'bundle' inside bitshares_acceptance_tests dir.
 
Next define environment variable BTS_BUILD with path to your bitshares toolkit's build directory, e.g.:

  $ export BTS_BUILD=/home/user/bitshares/bitshares_toolkit
  
  
## Usage
  
Bootstrap the test net:

  $ ruby testnet.rb -- create
  
After a couple of minutes the test net would be ready and you can run tests (features) via 'cucumber' command:

  $ cucumber
  
Or specifying a feature to run:

  $ cucumber features/market.feature
  
Or tag:

  $ cucumber -t @current


### Note

Testnet is being recreated from scratch before each scenario, so each scenario starts within a clean state.
  
If you want to pause scenarios execution and keep testnet running after scenario to inspect the nodes via http rpc, insert @pause tag before scenario.

In order to access the nodes via web wallet you need to create htdocs symlink to web_wallet/generated or to web_wallet/build, e.g. 

  $ ln -s ../web_wallet/generated htdocs
  
And open http://localhost:5690 (or 5691/5692) in your browser.
