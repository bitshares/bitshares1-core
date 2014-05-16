require 'sinatra/base'
require 'open3'
require 'fileutils'
require 'sinatra/reloader' if development?


class App < Sinatra::Base

  configure :development do
    register Sinatra::Reloader
    enable :logging, :dump_errors, :raise_errors, :show_exceptions
  end

  set :public_folder, 'generated'
  #set :public_folder, 'dist'
  set :sessions, true
  set :session_secret, 'AKFDOEJFGJAOEW'

  before do
    puts "Params: #{params.inspect}" if App.development? and !params.empty?
  end

  get '/' do
    File.read 'generated/index.html'
  end

  #def require_user
  #  redirect '/login' unless current_user
  #end
  #
  #def current_user
  #  return @current_user if @current_user
  #  return nil if session[:username].nil?
  #  @current_user = User.find(session[:username])
  #end

  #get '/app.js' do
  #  coffee :app
  #end

  #get '/login' do
  #  erb :login
  #end
  #
  #post '/login' do
  #  user = User.find(params[:username])
  #  if user and user[:password] == params[:password]
  #    session[:username] = user[:name]
  #    redirect '/'
  #  else
  #    session[:message] = 'Wrong user name or password, please try again.'
  #    erb :login
  #  end
  #end
  #
  #get '/logout' do
  #  @current_user = session[:username] = nil
  #  redirect '/login'
  #end

  #get '/' do
  #  require_user
  #  erb :index
  #end

end