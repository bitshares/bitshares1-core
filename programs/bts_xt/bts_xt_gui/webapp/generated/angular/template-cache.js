angular.module("app").run(["$templateCache", function($templateCache) {

  $templateCache.put("blocks.html",
    "<!--<ol class=\"breadcrumb\">-->\n" +
    " <!--<li><i class=\"fa fa-home fa-fw\"></i>Home</a></li>-->\n" +
    " <!--<li><i class=\"fa fa-link fa-fw\"></i> Block Explorer</li>-->\n" +
    "<!--</ol>-->\n" +
    "<div class=\"header\">\n" +
    "\t<div class=\"col-sm-12\">\n" +
    "\t\t<h3 class=\"header-title\">Block Explorer</h3>\n" +
    "\t</div>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"main-content\">\n" +
    "\n" +
    "</div>\n"
  );

  $templateCache.put("createwallet.html",
    "<br/>\n" +
    "<br/>\n" +
    "<div class=\"main-content\">\n" +
    "\n" +
    " <div class=\"header row\">\n" +
    "  <div class=\"col-md-12\">\n" +
    "   <h3 class=\"header-title\">BitShares XT Wallet</h3>\n" +
    "  </div>\n" +
    " </div>\n" +
    "<br/>\n" +
    " <form name=\"wform\" class=\"form-horizontal\" role=\"form\" ng-submit=\"submitForm(wform.$valid)\" novalidate>\n" +
    "\n" +
    "  <div class=\"row\">\n" +
    "   <div class=\"col-md-12\">\n" +
    "    <h3>Wallet Password</h3>\n" +
    "\n" +
    "    <p>Your wallet password is optional and secures your transaction history. This password is separate from your\n" +
    "     spending password which secures your private keys. If you forget this password you will be unable to view or\n" +
    "     transfer your shares.</p>\n" +
    "\n" +
    "    <div class=\"form-group\" ng-class=\"{ 'has-error' : wform.wp.$invalid && !wform.wp.$pristine }\">\n" +
    "     <label for=\"wallet_password\" class=\"col-sm-2 control-label\">Password</label>\n" +
    "\n" +
    "     <div class=\"col-sm-6\">\n" +
    "      <input ng-model=\"wallet_password\" name=\"wp\" type=\"password\" class=\"form-control\" id=\"wallet_password\"\n" +
    "             placeholder=\"Password\" autofocus ng-minlength=\"3\">\n" +
    "      <p ng-show=\"wform.wp.$error.minlength\" class=\"help-block\">Password is too short.</p>\n" +
    "     </div>\n" +
    "    </div>\n" +
    "    <div class=\"form-group\" ng-class=\"{ 'has-error' : wform.cwp.$invalid && !wform.cwp.$pristine }\">\n" +
    "     <label for=\"confirm_wallet_password\" class=\"col-sm-2 control-label\">Confirm Password</label>\n" +
    "\n" +
    "     <div class=\"col-sm-6\">\n" +
    "      <input ng-model=\"confirm_wallet_password\" name=\"cwp\" type=\"password\" class=\"form-control\"\n" +
    "             id=\"confirm_wallet_password\"\n" +
    "             placeholder=\"Confirm Password\" data-match=\"wallet_password\">\n" +
    "      <p ng-show=\"wform.cwp.$error.match\" class=\"help-block\">Fields do not match.</p>\n" +
    "     </div>\n" +
    "    </div>\n" +
    "   </div>\n" +
    "  </div>\n" +
    "  <br/>\n" +
    "  <div class=\"row\">\n" +
    "   <div class=\"col-md-12\">\n" +
    "    <h3>Spending Password</h3>\n" +
    "\n" +
    "    <p>Your spending password is manditory and controls when and how your funds may be spent. If you forget this\n" +
    "     password you will be unable to transfer your shares.</p>\n" +
    "\n" +
    "    <div class=\"form-group\" ng-class=\"{ 'has-error' : wform.sp.$invalid && !wform.sp.$pristine }\">\n" +
    "     <label for=\"spending_password\" class=\"col-sm-2 control-label\">Password</label>\n" +
    "\n" +
    "     <div class=\"col-sm-6\">\n" +
    "      <input ng-model=\"spending_password\" name=\"sp\" type=\"password\" class=\"form-control\" id=\"spending_password\"\n" +
    "             placeholder=\"Password\"\n" +
    "             required autofocus ng-minlength=\"9\">\n" +
    "\n" +
    "      <p ng-show=\"wform.sp.$error.minlength\" class=\"help-block\">Password is too short. It must be more than 8 characters.</p>\n" +
    "\n" +
    "      <p ng-show=\"wform.sp.$invalid && !wform.sp.$pristine\" class=\"help-block\">Password is required.</p>\n" +
    "     </div>\n" +
    "    </div>\n" +
    "    <div class=\"form-group\" ng-class=\"{ 'has-error' : wform.csp.$invalid && !wform.csp.$pristine }\">\n" +
    "     <label for=\"confirm_spending_password\" class=\"col-sm-2 control-label\">Confirm Password</label>\n" +
    "\n" +
    "     <div class=\"col-sm-6\">\n" +
    "      <input ng-model=\"confirm_spending_password\" name=\"csp\" type=\"password\" class=\"form-control\"\n" +
    "             id=\"confirm_spending_password\"\n" +
    "             placeholder=\"Confirm Password\" required data-match=\"spending_password\">\n" +
    "\n" +
    "      <p ng-show=\"wform.csp.$error.match\" class=\"help-block\">Fields do not match.</p>\n" +
    "     </div>\n" +
    "    </div>\n" +
    "\n" +
    "    <div class=\"form-group row\">\n" +
    "     <div class=\"col-sm-offset-2 col-sm-10\">\n" +
    "      <button type=\"submit\" class=\"btn btn-primary btn-lg\">Create Wallet</button>\n" +
    "     </div>\n" +
    "    </div>\n" +
    "   </div>\n" +
    "  </div>\n" +
    "\n" +
    "\n" +
    " </form>\n" +
    "\n" +
    "</div>\n"
  );

  $templateCache.put("home.html",
    "<!--<ol class=\"breadcrumb\">-->\n" +
    "<!--<li><i class=\"fa fa-home fa-fw\"></i> Overview</li>-->\n" +
    "<!--</ol>-->\n" +
    "\n" +
    "<!--<div class=\"header\">-->\n" +
    "<!--<div class=\"col-md-12\">-->\n" +
    "<!--<h3 class=\"header-title\">Dashboard</h3>-->\n" +
    "<!--<p class=\"header-info\">Overview and latest statistics</p>-->\n" +
    "<!--</div>-->\n" +
    "<!--</div>-->\n" +
    "\n" +
    "<div class=\"main-content\">\n" +
    " <div class=\"row\">\n" +
    "  <div class=\"col-sm-4\">\n" +
    "   <div class=\"panel\">\n" +
    "    <div class=\"panel-heading\">\n" +
    "     <h3 class=\"panel-title\">Balance {{balance | currency : ''}}</h3>\n" +
    "    </div>\n" +
    "    <div class=\"panel-body\">\n" +
    "     <div class=\"col-sm-4\">\n" +
    "     </div>\n" +
    "    </div>\n" +
    "   </div>\n" +
    "  </div>\n" +
    "\t<div class=\"col-sm-8\">\n" +
    "\t\t<h3>Latest Transactions</h3>\n" +
    "\t\t<table class=\"table table-striped table-hover\">\n" +
    "\t\t\t<thead>\n" +
    "\t\t\t<tr>\n" +
    "\t\t\t\t<th>TRX</th>\n" +
    "\t\t\t\t<th>CONFIRMED</th>\n" +
    "\t\t\t\t<th>AMOUNT</th>\n" +
    "\t\t\t\t<th>FROM</th>\n" +
    "\t\t\t\t<th>TO</th>\n" +
    "\t\t\t</tr>\n" +
    "\t\t\t</thead>\n" +
    "\t\t\t<tbody>\n" +
    "\t\t\t<tr ng-repeat=\"t in transactions\">\n" +
    "\t\t\t\t<td>{{ t.trx_num }}</td>\n" +
    "\t\t\t\t<td>{{ t.time }}</td>\n" +
    "\t\t\t\t<td>{{ t.amount | currency : '' }}</td>\n" +
    "\t\t\t\t<td>{{ t.from }}</td>\n" +
    "\t\t\t\t<td>{{ t.to }}</td>\n" +
    "\t\t\t</tr>\n" +
    "\t\t\t</tbody>\n" +
    "\t\t</table>\n" +
    "\t</div>\n" +
    " </div>\n" +
    "</div>\n"
  );

  $templateCache.put("openwallet.html",
    " <div class=\"modal-header\">\n" +
    "  <h3 class=\"modal-title\">{{ title }}</h3>\n" +
    " </div>\n" +
    " <form role=\"form\" ng-submit=\"ok()\">\n" +
    " <div class=\"modal-body\">\n" +
    "  <div class=\"form-group\" ng-class=\"{'has-error': $parent.has_error}\">\n" +
    "   <label for=\"wallet_password\">{{ password_label }}</label>\n" +
    "   <input ng-model=\"$parent.password\" focus-me type=\"password\" class=\"form-control\" id=\"wallet_password\" placeholder=\"Password\" required autofocus>\n" +
    "   <p class=\"help-block error\" ng-show=\"$parent.has_error\">{{ wrong_password_msg }}</p>\n" +
    "  </div>\n" +
    " </div>\n" +
    " </form>\n" +
    " <div class=\"modal-footer\">\n" +
    "  <button class=\"btn btn-primary\" ng-click=\"ok()\">OK</button>\n" +
    "  <button class=\"btn btn-warning\" ng-click=\"cancel()\">Cancel</button>\n" +
    " </div>\n"
  );

  $templateCache.put("receive.html",
    "<!--<ol class=\"breadcrumb\">-->\n" +
    "<!--<li><i class=\"fa fa-home fa-fw\"></i>Home</a></li>-->\n" +
    "<!--<li><i class=\"fa fa-sign-in fa-fw\"></i> Receive</li>-->\n" +
    "<!--</ol>-->\n" +
    "\n" +
    "<div class=\"main-content\">\n" +
    " <div class=\"row\">\n" +
    "  <div class=\"col-sm-12\">\n" +
    "\n" +
    "   <tabset>\n" +
    "\n" +
    "    <tab heading=\"Create Address\">\n" +
    "     <br/>\n" +
    "     <form class=\"form-horizontal\" role=\"form\" ng-submit=\"create_address()\">\n" +
    "      <div class=\"form-group\">\n" +
    "       <label for=\"new_address_label\" class=\"col-sm-2 control-label\">Label</label>\n" +
    "       <div class=\"col-sm-9\">\n" +
    "        <input ng-model=\"$parent.new_address_label\" type=\"text\" class=\"form-control\" id=\"new_address_label\" placeholder=\"Label\"\n" +
    "               autofocus>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "      <div class=\"form-group\">\n" +
    "       <div class=\"col-sm-offset-2 col-sm-10\">\n" +
    "        <button type=\"submit\" class=\"btn btn-primary\">Create Address</button>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "     </form>\n" +
    "    </tab>\n" +
    "\n" +
    "    <tab heading=\"Import Key\">\n" +
    "     <br/>\n" +
    "     <form class=\"form-horizontal\" role=\"form\" ng-submit=\"import_key()\">\n" +
    "      <div class=\"form-group\">\n" +
    "       <label for=\"pk_label\" class=\"col-sm-2 control-label\">Label</label>\n" +
    "       <div class=\"col-sm-9\">\n" +
    "        <input ng-model=\"$parent.pk_label\" type=\"text\" class=\"form-control\" id=\"pk_label\" placeholder=\"Label\"/>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "      <div class=\"form-group\">\n" +
    "       <label for=\"pk_value\" class=\"col-sm-2 control-label\">Private Key</label>\n" +
    "       <div class=\"col-sm-9\">\n" +
    "        <input ng-model=\"$parent.pk_value\" type=\"text\" class=\"form-control\" id=\"pk_value\" placeholder=\"Private Key\"/>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "      <div class=\"form-group\">\n" +
    "       <div class=\"col-sm-offset-2 col-sm-10\">\n" +
    "        <button type=\"submit\" class=\"btn btn-primary\">Import Key</button>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "     </form>\n" +
    "    </tab>\n" +
    "\n" +
    "    <tab heading=\"Import Wallet\">\n" +
    "     <br/>\n" +
    "\n" +
    "     <form class=\"form-horizontal\" role=\"form\" ng-submit=\"import_wallet()\">\n" +
    "      <div class=\"form-group\">\n" +
    "       <label for=\"wallet_file\" class=\"col-sm-2 control-label\">Wallet Path</label>\n" +
    "       <div class=\"col-sm-9\">\n" +
    "        <input ng-model=\"$parent.wallet_file\" type=\"text\" class=\"form-control\" id=\"wallet_file\" placeholder=\"\"/>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "      <div class=\"form-group\">\n" +
    "       <label for=\"wallet_password\" class=\"col-sm-2 control-label\">Wallet Password</label>\n" +
    "       <div class=\"col-sm-9\">\n" +
    "        <input ng-model=\"$parent.wallet_password\" type=\"password\" class=\"form-control\" id=\"wallet_password\" placeholder=\"Password\"/>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "      <div class=\"form-group\">\n" +
    "       <div class=\"col-sm-offset-2 col-sm-10\">\n" +
    "        <button type=\"submit\" class=\"btn btn-primary\">Import Wallet</button>\n" +
    "       </div>\n" +
    "      </div>\n" +
    "     </form>\n" +
    "    </tab>\n" +
    "\n" +
    "   </tabset>\n" +
    "\n" +
    "  </div>\n" +
    " </div>\n" +
    "\n" +
    " <br/>\n" +
    " <div class=\"row\">\n" +
    " <div class=\"col-sm-12\">\n" +
    "  <h3>Receive Addresses</h3>\n" +
    "  <table class=\"table table-striped table-hover\">\n" +
    "   <thead>\n" +
    "   <tr>\n" +
    "    <th>Label</th>\n" +
    "    <th>Address</th>\n" +
    "   </tr>\n" +
    "   </thead>\n" +
    "   <tbody>\n" +
    "   <tr ng-repeat=\"a in addresses\">\n" +
    "    <td>{{a.label}}</td>\n" +
    "    <td>{{a.address}}</td>\n" +
    "   </tr>\n" +
    "   </tbody>\n" +
    "  </table>\n" +
    " </div>\n" +
    " </div>\n" +
    "\n" +
    "</div>\n"
  );

  $templateCache.put("transactions.html",
    "<!--<ol class=\"breadcrumb\">-->\n" +
    "<!--<li><i class=\"fa fa-home fa-fw\"></i>Home</a></li>-->\n" +
    "<!--<li><i class=\"fa fa-exchange fa-fw\"></i> Transaction History</li>-->\n" +
    "<!--</ol>-->\n" +
    "<div class=\"header\">\n" +
    "\t<div class=\"col-sm-12\">\n" +
    "\t\t<h3 class=\"header-title\">Transactions</h3>\n" +
    "\t</div>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"main-content\">\n" +
    "\n" +
    "\t<div id=\"transaction_history\">\n" +
    "\t\t<!--<input type=\"button\" ng-click=\"rescan()\" value=\"Rescan\" class=\"btn btn-primary pull-right\"/>-->\n" +
    "\t\t<table class=\"table table-striped table-hover\">\n" +
    "\t\t\t<thead>\n" +
    "\t\t\t<tr>\n" +
    "\t\t\t\t<th>BLK</th>\n" +
    "\t\t\t\t<th>TRX</th>\n" +
    "\t\t\t\t<th>CONFIRMED</th>\n" +
    "\t\t\t\t<th>AMOUNT</th>\n" +
    "\t\t\t\t<th>FROM</th>\n" +
    "\t\t\t\t<th>TO</th>\n" +
    "\t\t\t</tr>\n" +
    "\t\t\t</thead>\n" +
    "\t\t\t<tbody>\n" +
    "\t\t\t<tr ng-repeat=\"t in transactions\">\n" +
    "\t\t\t\t<td>{{ t.block_num }}</td>\n" +
    "\t\t\t\t<td>{{ t.trx_num }}</td>\n" +
    "\t\t\t\t<td>{{ t.time }}</td>\n" +
    "\t\t\t\t<td>{{ t.amount | currency : '' }}</td>\n" +
    "\t\t\t\t<td>{{ t.from }}</td>\n" +
    "\t\t\t\t<td>{{ t.to }}</td>\n" +
    "\t\t\t</tr>\n" +
    "\t\t\t</tbody>\n" +
    "\t\t</table>\n" +
    "\t</div>\n" +
    "\n" +
    "</div>\n" +
    "\n" +
    "</div>\n"
  );

  $templateCache.put("transfer.html",
    "<!--<ol class=\"breadcrumb\">-->\n" +
    " <!--<li><i class=\"fa fa-home fa-fw\"></i>Home</a></li>-->\n" +
    " <!--<li><i class=\"fa fa-sign-out fa-fw\"></i> Transfer</li>-->\n" +
    "<!--</ol>-->\n" +
    "\n" +
    "<div class=\"header\">\n" +
    " <div class=\"col-md-12\">\n" +
    "  <h3 class=\"header-title\">Transfer</h3>\n" +
    " </div>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"main-content\">\n" +
    "\n" +
    " <div id=\"transfer-form\">\n" +
    "  <form class=\"form-horizontal\" role=\"form\" ng-submit=\"send()\">\n" +
    "   <div class=\"form-group\">\n" +
    "    <label for=\"payto\" class=\"col-sm-2 control-label\">To</label>\n" +
    "\n" +
    "    <div class=\"col-sm-10\">\n" +
    "     <input ng-model=\"payto\" type=\"text\" class=\"form-control\" placeholder=\"Address\" id=\"payto\"/>\n" +
    "    </div>\n" +
    "   </div>\n" +
    "   <div class=\"form-group\">\n" +
    "    <label for=\"amount\" class=\"col-sm-2 control-label\">Amount</label>\n" +
    "\n" +
    "    <div class=\"col-sm-10\">\n" +
    "     <input ng-model=\"amount\" type=\"number\" class=\"form-control\" placeholder=\"0.0\" id=\"amount\"/>\n" +
    "    </div>\n" +
    "   </div>\n" +
    "   <div class=\"form-group\">\n" +
    "    <label for=\"memo\" class=\"col-sm-2 control-label\">Memo</label>\n" +
    "    <div class=\"col-sm-10\">\n" +
    "     <input ng-model=\"memo\" type=\"text\" class=\"form-control\" placeholder=\"Memo\" id=\"memo\"/>\n" +
    "    </div>\n" +
    "   </div>\n" +
    "\t\t<div class=\"form-group row\">\n" +
    "\t\t\t<div class=\"col-sm-offset-2 col-sm-10\">\n" +
    "\t\t\t\t<button type=\"submit\" class=\"btn btn-primary btn-lg\">Send</button>\n" +
    "\t\t\t</div>\n" +
    "\t\t</div>\n" +
    "  </form>\n" +
    " </div>\n" +
    "\n" +
    "</div>\n" +
    "\n" +
    "</div>\n"
  );

}]);
