require 'erb'
require 'json'

class Generator

  attr_reader :wallet_api, :network_api

  def initialize
    @wallet_api = JSON.parse(IO.read('../wallet_api.json'))
    @network_api = JSON.parse(IO.read('../network_api.json'))
  end

  def render(template)
    ERB.new(template).result(binding)
  end

private

  # helper methods used in erb templates
  def js_method_params(method)
    param_names = method['parameters'].map{ |p| p['name'] }
    return '' if param_names.empty?
    '(' + param_names.join(', ') + ')'
  end

  def js_pass_params(method)
    param_names = method['parameters'].map { |p| "'#{p['name']}'" }
    return '' if param_names.empty?
    ', [' + param_names.join(', ') + ']'
  end

end

generator = Generator.new

Dir['./templates/*.tmpl'].each do |template_file|
  unless template_file =~ /templates\/(.+)\.tmpl$/
    puts "template '#{template_file}' couldn't be processed"
    next
  end
  output_file = "./output/#{$1}"
  print "processing #{template_file} -> #{output_file} ..  "
  $stdout.flush
  template = IO.read(template_file)
  res = generator.render(template)
  IO.write(output_file, res)
  puts 'done'
end
