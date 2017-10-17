library("reshape2")
library("plyr")
library("ggplot2")

for(i in (1:100)){
  
filename = paste('bandwidth',i,'.csv', sep="")

if(file.exists(filename)){

  bandwidth = read.csv(filename, header = TRUE)
  
  bw_plot = ggplot(data = bandwidth, aes(x=time, y=bytes, group=1)) + geom_line()
  
  png(filename=paste('bandwidth',i,'.png', sep=""))
  plot(bw_plot)
  dev.off
}

}

