class Ring
  include Enumerable

  def initialize max
    @max = max
    @buffer = []
  end

  def << element
    @buffer.pop if @buffer.size >= @max
    @buffer.unshift(element)
  end

  def each(&block)
    @buffer.each(&block)
  end
end
