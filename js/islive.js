import fetch from 'node-fetch';
import { JSDOM } from 'jsdom';
import * as fs from 'fs';

async function getVideoType(videoUrl) {
  const response = await fetch(videoUrl);
  const html = await response.text();
  const dom = new JSDOM(html);
  const document = dom.window.document;
  fs.writeFile(`../build/fetch.html`, html, (err) => {
    console.error(err);
  })

  //const infoStrings = document.querySelector('body')
  //console.log(html)
  if (html) {
    const infoText = html.trim();
    console.log(infoText);
    // Find the "isLive" JSON substring
    const isLiveStartIndex = infoText.indexOf('isLive');
    if (isLiveStartIndex !== -1) {
      const isLiveEndIndex = infoText.indexOf('}', isLiveStartIndex);
      const isLiveJSON = infoText.substring(isLiveStartIndex-1, isLiveEndIndex + 1);
      
      // Parse the "isLive" JSON
      const isLiveData = JSON.parse(isLiveJSON);
      if (isLiveData.isLive) {
        return 'live';
      } else {
        // Check for "isLiveNow" in the JSON
        if (isLiveData.isLiveNow) {
          return 'live';
        } else {
          return 'video';
        }
      }
    } else if (infoText.startsWith('Streamed')) {
      return 'stream';
    } else {
      return 'video';
    }
  } else {
    return 'unknown';
  }
}
const videoUrl = 'https://www.youtube.com/watch?v=pcs2xMTaTu4';
getVideoType(videoUrl)
  .then((type) => {
    console.log(type);
  })
  .catch((error) => {
    console.error(error);
  });
