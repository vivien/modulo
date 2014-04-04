# Fetch last few tweets, unauthenticated.
#
# I just want the last few tweets of a user, I really don't want to do this:
# http://stackoverflow.com/a/15314662

require 'cgi'
require 'open-uri'
require 'openssl'

module Twitter

  module_function

  def last_tweets(user)
    url = "https://twitter.com/" << user
    html = open(url, :ssl_verify_mode => OpenSSL::SSL::VERIFY_NONE).read
    html.scan(/<p class="js-tweet-text tweet-text">(.*)<\/p>/).map do |p|
      p = p.first.gsub(/<\/?[^>]*>/, '')  # remove HTML elements
      p = CGI.unescape_html(p)            # remove some HTML sequences like encoded '
      p.gsub('&nbsp;', '')                # unescape doesn't remove them
    end
  rescue OpenURI::HTTPError
    []
  end

end
