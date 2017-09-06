package com.perpendox.phem;

public class FileChooserItem implements Comparable<FileChooserItem>{
    private String name;
    private String data;
    private String date;
    private String path;
    private String image;
    private int type;
    public static final int TYPE_PARENT = 101;
    public static final int TYPE_CURRENT = 102;
    public static final int TYPE_DIR = 103;
    public static final int TYPE_FILE = 104;
   
    public FileChooserItem(String n,String d, String dt, String p, String img, int t)
    {
            name = n;
            data = d;
            date = dt;
            path = p;
            image = img;
            type = t;
    }
    public String getName()
    {
            return name;
    }
    public String getData()
    {
            return data;
    }
    public String getDate()
    {
            return date;
    }
    public String getPath()
    {
            return path;
    }
    public String getImage() {
            return image;
    }
    public int getType() {
        return type;
    }
    public int compareTo(FileChooserItem o) {
            if(this.name != null)
                    return this.name.toLowerCase().compareTo(o.getName().toLowerCase());
            else
                    throw new IllegalArgumentException();
    }
}