require 'json'
require 'net/http'
require 'open-uri'
require 'openssl'
require 'uri'
require 'zlib'

module URL

  module_function

  def get_title url
    stream = open(URI.escape(url), ssl_verify_mode: OpenSSL::SSL::VERIFY_NONE)
    if (stream.content_encoding.include? "gzip")
      body = Zlib::GzipReader.new(stream).read
    else
      body = stream.read
    end
    body =~ /<title>(.+)<\/title>/
    $1
  rescue => e # we don't want the script to break so rescue anything, for robustness
  #rescue SocketError, OpenURI::HTTPError => e
    STDERR.puts "#{ e.message } (#{ url })"
    nil
  end

  def shorten url
    Shorten.isgd url rescue nil
  end

  module Shorten

    # TODO find a shortening service which keeps the file extension

    module_function

    def isgd url
      isgd = "http://is.gd/create.php?format=simple&url=#{ URI.escape(url) }"
      open(isgd).read
    end

    def pastis url
      uri = URI.parse("http://past.is/api/") # trailing slash is important!
      response = Net::HTTP.post_form(uri, url: url)
      pastis = JSON.parse(response.body)
      pastis["shorturl"]
    end

  end # Shorten

end # URL
